#include <gtk/gtk.h>
#include <gdal.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "gui.h"
#include "gui_utils.h"
#include "data_loader.h"
#include "resampler.h"
#include "utils.h"
#include "index_calculator.h"
#include "processing_pipeline.h"
#include "visualization.h"

#define DEFAULT_WINDOW_WIDTH 900
#define DEFAULT_WINDOW_HEIGHT 750

typedef struct
{
    float* ndvi_data;
    float* ndmi_data;
    int width;
    int height;
    char current_map_type[10];
} IndexMapData;

typedef struct
{
    GtkApplication* app;
    IndexMapData* map_data;
} MapWindowData;

// Zmienne globalne
static char* path_b04 = NULL;
static char* path_b08 = NULL;
static char* path_b11 = NULL;
static char* path_scl = NULL;
static gboolean res_10m_selected = TRUE;

static GtkWidget* btn_load_b04_widget = NULL;
static GtkWidget* btn_load_b08_widget = NULL;
static GtkWidget* btn_load_b11_widget = NULL;
static GtkWidget* btn_load_scl_widget = NULL;

// Statyczny wskaźnik do okna konfiguracji
static GtkWidget* the_config_window = NULL;

// ====== GUI - GŁÓWNE FUNKCJE ======
static void activate_config_window(GtkApplication* app);
static GtkWidget* create_map_window(GtkApplication* app, IndexMapData* map_data);
static gboolean on_draw_map_area(GtkWidget* widget, cairo_t* cr);

// ====== GUI - OBSŁUGA ZDARZEŃ ======
static void on_config_window_destroy(GtkWidget* widget);
static void on_load_b04_clicked(GtkWidget* widget, gpointer data);
static void on_load_b08_clicked(GtkWidget* widget, gpointer data);
static void on_load_b11_clicked(GtkWidget* widget, gpointer data);
static void on_load_scl_clicked(GtkWidget* widget, gpointer data);
static void on_rozpocznij_clicked(GtkWidget* widget, gpointer user_data);
static void on_config_radio_resolution_toggled(GtkToggleButton* togglebutton);
static void on_map_type_radio_toggled(GtkToggleButton* togglebutton, gpointer user_data);

// ====== WALIDACJA ======
static int validate_band_paths(BandData bands[], int band_count, GtkWindow* parent_window);

// ====== PRZETWARZANIE DANYCH ======
static IndexMapData* convert_processing_result_to_map_data(ProcessingResult* processing_result);

// ====== PAMIĘĆ ======
static void free_index_map_data(gpointer data);
static void map_window_data_destroy(gpointer data);

// ====== IMPLEMENTACJE - GUI GŁÓWNE ======

static void activate_config_window(GtkApplication* app)
{
    if (the_config_window)
    {
        if (gtk_widget_get_visible(the_config_window))
        {
            gtk_window_present(GTK_WINDOW(the_config_window));
        }
        else
        {
            gtk_widget_show_all(the_config_window);
            gtk_window_present(GTK_WINDOW(the_config_window));
        }
        return;
    }

    GtkWidget* config_window_local;
    GtkWidget* main_vbox;
    GtkWidget* radio_hbox_config;
    GtkWidget* load_buttons_hbox;
    GtkWidget *radio_10m, *radio_20m;
    GtkWidget* btn_rozpocznij;

    config_window_local = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(config_window_local), "Konfiguracja Analizy");
    gtk_window_set_default_size(GTK_WINDOW(config_window_local), DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT);
    gtk_container_set_border_width(GTK_CONTAINER(config_window_local), 10);

    the_config_window = config_window_local;
    g_signal_connect(the_config_window, "destroy", G_CALLBACK(on_config_window_destroy), NULL);

    main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(config_window_local), main_vbox);

    radio_hbox_config = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(radio_hbox_config, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(main_vbox), radio_hbox_config, FALSE, FALSE, 0);

    radio_10m = gtk_radio_button_new_with_label(NULL, "upscaling do 10m");
    g_signal_connect(radio_10m, "toggled", G_CALLBACK(on_config_radio_resolution_toggled), NULL);
    gtk_box_pack_start(GTK_BOX(radio_hbox_config), radio_10m, FALSE, FALSE, 0);

    radio_20m = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio_10m), "downscaling do 20m");
    g_signal_connect(radio_20m, "toggled", G_CALLBACK(on_config_radio_resolution_toggled), NULL);
    gtk_box_pack_start(GTK_BOX(radio_hbox_config), radio_20m, FALSE, FALSE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_10m), TRUE);

    load_buttons_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_set_homogeneous(GTK_BOX(load_buttons_hbox), TRUE);
    gtk_box_pack_start(GTK_BOX(main_vbox), load_buttons_hbox, FALSE, FALSE, 0);

    btn_load_b04_widget = create_button_with_ellipsis("Wczytaj pasmo B04");
    g_signal_connect(btn_load_b04_widget, "clicked", G_CALLBACK(on_load_b04_clicked), config_window_local);
    gtk_box_pack_start(GTK_BOX(load_buttons_hbox), btn_load_b04_widget, TRUE, TRUE, 0);

    btn_load_b08_widget = create_button_with_ellipsis("Wczytaj pasmo B08");
    g_signal_connect(btn_load_b08_widget, "clicked", G_CALLBACK(on_load_b08_clicked), config_window_local);
    gtk_box_pack_start(GTK_BOX(load_buttons_hbox), btn_load_b08_widget, TRUE, TRUE, 0);

    btn_load_b11_widget = create_button_with_ellipsis("Wczytaj pasmo B11");
    g_signal_connect(btn_load_b11_widget, "clicked", G_CALLBACK(on_load_b11_clicked), config_window_local);
    gtk_box_pack_start(GTK_BOX(load_buttons_hbox), btn_load_b11_widget, TRUE, TRUE, 0);

    btn_load_scl_widget = create_button_with_ellipsis("Wczytaj pasmo SCL");
    g_signal_connect(btn_load_scl_widget, "clicked", G_CALLBACK(on_load_scl_clicked), config_window_local);
    gtk_box_pack_start(GTK_BOX(load_buttons_hbox), btn_load_scl_widget, TRUE, TRUE, 0);

    btn_rozpocznij = gtk_button_new_with_label("Rozpocznij");
    g_signal_connect(btn_rozpocznij, "clicked", G_CALLBACK(on_rozpocznij_clicked), config_window_local);
    gtk_box_pack_start(GTK_BOX(main_vbox), btn_rozpocznij, FALSE, FALSE, 0);
    gtk_widget_set_halign(btn_rozpocznij, GTK_ALIGN_START);

    gtk_widget_show_all(config_window_local);
}

static GtkWidget* create_map_window(GtkApplication* app, IndexMapData* map_data)
{
    if (!map_data)
    {
        g_printerr("[%s] Błąd tworzenia mapy --- Brak wskaźnika do mapy.\n", get_timestamp());
        return NULL;
    }

    if (!map_data->ndvi_data && !map_data->ndmi_data)
    {
        g_printerr("[%s] Błąd tworzenia mapy --- Brak danych NDMI / NDVI.\n", get_timestamp());
        free_index_map_data(map_data);
        return NULL;
    }

    // Tworzenie okna
    GtkWidget* map_window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(map_window), "Wynikowa Mapa Wskaźników");
    gtk_window_set_default_size(GTK_WINDOW(map_window), DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT);
    gtk_container_set_border_width(GTK_CONTAINER(map_window), 10);

    // Enkapsulacja danych okna z automatycznym cleanup
    MapWindowData* window_data = g_new(MapWindowData, 1);
    window_data->app = app;
    window_data->map_data = map_data;

    g_object_set_data_full(G_OBJECT(map_window), "window_data",
                           window_data, map_window_data_destroy);

    // Automatyczna reaktywacja aplikacji przy zamykaniu okna
    g_signal_connect_swapped(map_window, "destroy",
                             G_CALLBACK(g_application_activate), app);

    // Tworzenie UI
    GtkWidget* main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(map_window), main_vbox);

    // Radio buttons dla wyboru mapy
    GtkWidget* radio_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(radio_hbox, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(main_vbox), radio_hbox, FALSE, FALSE, 0);

    // Drawing area
    GtkWidget* drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(drawing_area, map_data->width, map_data->height);

    // Przypisanie danych do drawing_area (dla funkcji rysowania)
    g_object_set_data(G_OBJECT(drawing_area), "map_data_ptr", map_data);
    g_signal_connect(drawing_area, "draw", G_CALLBACK(on_draw_map_area), map_data);

    // Radio buttons
    GtkWidget* radio_ndmi = gtk_radio_button_new_with_label(NULL, "NDMI");
    gtk_box_pack_start(GTK_BOX(radio_hbox), radio_ndmi, FALSE, FALSE, 0);
    g_signal_connect(radio_ndmi, "toggled", G_CALLBACK(on_map_type_radio_toggled), drawing_area);

    GtkWidget* radio_ndvi = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio_ndmi), "NDVI");
    gtk_box_pack_start(GTK_BOX(radio_hbox), radio_ndvi, FALSE, FALSE, 0);
    g_signal_connect(radio_ndvi, "toggled", G_CALLBACK(on_map_type_radio_toggled), drawing_area);

    // Ustawienie domyślnego wyboru
    if (strcmp(map_data->current_map_type, "NDVI") == 0)
    {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_ndvi), TRUE);
    }
    else
    {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_ndmi), TRUE);
    }

    // Scrolled window dla drawing area
    GtkWidget* scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand(scrolled_window, TRUE);
    gtk_widget_set_vexpand(scrolled_window, TRUE);
    gtk_box_pack_start(GTK_BOX(main_vbox), scrolled_window, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(scrolled_window), drawing_area);

    return map_window;
}

static gboolean on_draw_map_area(GtkWidget* widget, cairo_t* cr)
{
    IndexMapData* map_data = g_object_get_data(G_OBJECT(widget), "map_data_ptr");

    if (!map_data)
    {
        g_printerr("[%s] Błąd tworzenia mapy --- Brak wskaźnika do danych mapy. Rysowanie tła.\n", get_timestamp());
        cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
        cairo_paint(cr);
        return TRUE;
    }

    const float* data_to_render = strcmp(map_data->current_map_type, "NDVI") == 0
                                      ? map_data->ndvi_data
                                      : map_data->ndmi_data;
    if (!data_to_render)
    {
        g_printerr("[%s] Błąd tworzenia mapy --- Brak wskaźnika do mapy %s.\n", get_timestamp(),
                   map_data->current_map_type);
        cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
        cairo_paint(cr);
        return TRUE;
    }

    GdkPixbuf* pixbuf = generate_pixbuf_from_index_data(data_to_render,
                                                        map_data->width,
                                                        map_data->height);
    if (pixbuf)
    {
        gdk_cairo_set_source_pixbuf(cr, pixbuf, 0, 0);
        cairo_paint(cr);
        g_object_unref(pixbuf);
    }
    else
    {
        g_printerr("[%s] Błąd tworzenia mapy --- Nie udało się wygenerować pixbuf.\n", get_timestamp());
        cairo_set_source_rgb(cr, 1, 0, 0);
        cairo_paint(cr);
    }
    return TRUE;
}

int run_gui(int argc, char* argv[])
{
    GtkApplication* app;
    int status;

    GDALAllRegister();

    app = gtk_application_new("org.projekt.sentinelgui", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate_config_window), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    GDALDestroyDriverManager();

    if (path_b04) g_free(path_b04);
    if (path_b08) g_free(path_b08);
    if (path_b11) g_free(path_b11);
    if (path_scl) g_free(path_scl);

    g_print("Aplikacja zakończona, status: %d\n", status);
    return status;
}

// ====== IMPLEMENTACJE - OBSŁUGA ZDARZEŃ ======

static void on_config_window_destroy(GtkWidget* widget)
{
    if (the_config_window == widget)
    {
        the_config_window = NULL;
    }
}

static void on_load_b04_clicked(GtkWidget* widget, gpointer data)
{
    handle_file_selection(GTK_WINDOW(data), "pasma B04", &path_b04, GTK_BUTTON(widget));
}

static void on_load_b08_clicked(GtkWidget* widget, gpointer data)
{
    handle_file_selection(GTK_WINDOW(data), "pasma B08", &path_b08, GTK_BUTTON(widget));
}

static void on_load_b11_clicked(GtkWidget* widget, gpointer data)
{
    handle_file_selection(GTK_WINDOW(data), "pasma B11", &path_b11, GTK_BUTTON(widget));
}

static void on_load_scl_clicked(GtkWidget* widget, gpointer data)
{
    handle_file_selection(GTK_WINDOW(data), "pasma SCL", &path_scl, GTK_BUTTON(widget));
}

static void on_rozpocznij_clicked(GtkWidget* widget, gpointer user_data)
{
    GtkWidget* config_window_widget = GTK_WIDGET(user_data);
    GtkWindow* parent_gtk_window = GTK_WINDOW(user_data);

    // Przygotowanie danych pasm
    int widths[4], heights[4];
    float* raw_data[4] = {NULL};
    float* processed_data[4] = {NULL};

    BandData bands[4] = {
        {&path_b04, &btn_load_b04_widget, &raw_data[0], &processed_data[0], &widths[0], &heights[0], "B04", 0},
        {&path_b08, &btn_load_b08_widget, &raw_data[1], &processed_data[1], &widths[1], &heights[1], "B08", 1},
        {&path_b11, &btn_load_b11_widget, &raw_data[2], &processed_data[2], &widths[2], &heights[2], "B11", 2},
        {&path_scl, &btn_load_scl_widget, &raw_data[3], &processed_data[3], &widths[3], &heights[3], "SCL", 3}
    };

    // Walidacja ścieżek plików
    if (!validate_band_paths(bands, 4, parent_gtk_window))
    {
        return;
    }

    g_print("[%s] Rozpoczynanie przetwarzania danych pasm.\n", get_timestamp());

    // Przetwarzanie przez pipeline
    ProcessingResult* processing_result = process_bands_and_calculate_indices(bands, res_10m_selected);

    if (!processing_result)
    {
        g_printerr("[%s] Wystąpił błąd podczas przetwarzania danych.\n", get_timestamp());
        show_error_dialog(parent_gtk_window, "Błąd podczas przetwarzania danych. Sprawdź konsolę dla szczegółów.");
        return;
    }

    // Konwersja wyniku na format GUI
    IndexMapData* map_display_data = convert_processing_result_to_map_data(processing_result);
    if (!map_display_data)
    {
        g_printerr("[%s] Błąd podczas konwersji danych dla GUI.\n", get_timestamp());
        show_error_dialog(parent_gtk_window, "Błąd podczas przygotowywania danych do wyświetlenia.");
        return;
    }

    // Tworzenie okna mapy
    GtkApplication* app = gtk_window_get_application(GTK_WINDOW(config_window_widget));
    GtkWidget* map_window = create_map_window(app, map_display_data);

    if (map_window)
    {
        g_print("[%s] Pomyślnie utworzono okno mapy.\n", get_timestamp());
        gtk_widget_show_all(map_window);
        gtk_widget_destroy(config_window_widget);
    }
    else
    {
        g_printerr("[%s] Błąd podczas tworzenia okna mapy.\n", get_timestamp());
        show_error_dialog(parent_gtk_window, "Błąd podczas tworzenia okna mapy.");
        free_index_map_data(map_display_data);
    }
}

static void on_config_radio_resolution_toggled(GtkToggleButton* togglebutton)
{
    if (gtk_toggle_button_get_active(togglebutton))
    {
        const gchar* label = gtk_button_get_label(GTK_BUTTON(togglebutton));
        res_10m_selected = g_str_has_prefix(label, "upscaling") ? TRUE : FALSE;
    }
}

static void on_map_type_radio_toggled(GtkToggleButton* togglebutton, gpointer user_data)
{
    if (gtk_toggle_button_get_active(togglebutton))
    {
        GtkWidget* drawing_area = GTK_WIDGET(user_data);
        IndexMapData* map_data = g_object_get_data(G_OBJECT(drawing_area), "map_data_ptr");
        const gchar* label = gtk_button_get_label(GTK_BUTTON(togglebutton));

        if (g_strcmp0(label, "NDVI") == 0)
        {
            strcpy(map_data->current_map_type, "NDVI");
        }
        else
        {
            strcpy(map_data->current_map_type, "NDMI");
        }

        if (GTK_IS_WIDGET(drawing_area) && gtk_widget_get_realized(drawing_area) &&
            gtk_widget_get_visible(drawing_area))
        {
            gtk_widget_queue_draw(drawing_area);
        }
    }
}

// ====== IMPLEMENTACJE - WALIDACJA ======

static int validate_band_paths(BandData bands[], int band_count, GtkWindow* parent_window)
{
    for (int i = 0; i < band_count; i++)
    {
        if (!*(bands[i].path))
        {
            GtkWidget* error_dialog = gtk_message_dialog_new(parent_window, GTK_DIALOG_DESTROY_WITH_PARENT,
                                                             GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                                                             "Błąd: Nie wszystkie pasma zostały wybrane!\n\nProszę wybrać pliki dla pasm B04, B08, B11 oraz SCL.");
            gtk_window_set_title(GTK_WINDOW(error_dialog), "Błąd wyboru plików");
            gtk_dialog_run(GTK_DIALOG(error_dialog));
            gtk_widget_destroy(error_dialog);
            return 0;
        }
    }
    return 1;
}

// ====== IMPLEMENTACJE - PRZETWARZANIE DANYCH ======
static IndexMapData* convert_processing_result_to_map_data(ProcessingResult* processing_result)
{
    if (!processing_result)
    {
        return NULL;
    }

    IndexMapData* map_data = g_new(IndexMapData, 1);
    if (!map_data)
    {
        g_printerr("[%s] Błąd alokacji pamięci dla IndexMapData.\n", get_timestamp());
        return NULL;
    }

    // Przeniesienie danych z ProcessingResult do IndexMapData
    map_data->ndvi_data = processing_result->ndvi_data;
    map_data->ndmi_data = processing_result->ndmi_data;
    map_data->width = processing_result->width;
    map_data->height = processing_result->height;
    strcpy(map_data->current_map_type, "NDVI");

    // Wyzerowanie wskaźników w ProcessingResult, żeby nie zostały zwolnione
    processing_result->ndvi_data = NULL;
    processing_result->ndmi_data = NULL;

    // Zwolnienie struktury ProcessingResult (ale nie danych, które przejęliśmy)
    free(processing_result);

    return map_data;
}

// ====== IMPLEMENTACJE - PAMIĘĆ ======

static void free_index_map_data(gpointer data)
{
    g_print("[%s] Rozpoczęto zwalnianie danych mapy.\n", get_timestamp());
    if (data == NULL)
    {
        g_print("[%s] Mapa nie zawiera danych, kończenie.\n", get_timestamp());
        return;
    }

    IndexMapData* map_data = data;

    if (map_data->ndvi_data)
    {
        free(map_data->ndvi_data);
        map_data->ndvi_data = NULL;
    }

    if (map_data->ndmi_data)
    {
        free(map_data->ndmi_data);
        map_data->ndmi_data = NULL;
    }

    g_free(map_data);
    g_print("[%s] Zakończono zwalnianie danych mapy.\n", get_timestamp());
}

static void map_window_data_destroy(gpointer data)
{
    g_print("[%s] map_window_data_destroy: Rozpoczynanie cleanup.\n", get_timestamp());

    MapWindowData* window_data = (MapWindowData*)data;
    if (!window_data)
    {
        g_print("[%s] window_data jest NULL, pomijam cleanup.\n", get_timestamp());
        return;
    }

    if (window_data->map_data)
    {
        g_print("[%s] Zwalnianie map_data: %p\n", get_timestamp(), (void*)window_data->map_data);
        free_index_map_data(window_data->map_data);
        window_data->map_data = NULL;
    }

    g_free(window_data);
    g_print("[%s] map_window_data_destroy: Cleanup zakończony.\n", get_timestamp());

    // Resetowanie ustawień domyślnych
    path_b04 = NULL;
    path_b08 = NULL;
    path_b11 = NULL;
    path_scl = NULL;
    res_10m_selected = TRUE;
}
