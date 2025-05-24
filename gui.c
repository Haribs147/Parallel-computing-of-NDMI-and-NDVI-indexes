#include <gtk/gtk.h>
#include <gdal.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <string.h>

#include "gui.h"
#include "gui_utils.h"
#include "data_loader.h"
#include "resampler.h"
#include "utils.h"
#include "index_calculator.h"
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

// Deklaracje wprzód
static void activate_config_window(GtkApplication* app, gpointer user_data);
static gboolean on_draw_map_area(GtkWidget* widget, cairo_t* cr, gpointer user_data);
static void on_config_window_destroy(GtkWidget* widget, gpointer data);
static GtkWidget* create_map_window(GtkApplication* app, IndexMapData* map_data);

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

// Funkcje obsługi zdarzeń dla przycisków ładowania
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

static int load_all_bands_data(BandData bands[4])
{
    // Wczytywanie danych dla wszystkich pasm
    for (int i = 0; i < 4; i++)
    {
        *(bands[i].raw_data) = LoadBandData(*(bands[i].path), bands[i].width, bands[i].height);
        if (!*(bands[i].raw_data))
        {
            fprintf(stderr, "Błąd wczytywania %s!\n", bands[i].band_name);
            return -1;
        }
        *(bands[i].processed_data) = *(bands[i].raw_data);
    }
    return 0;
}

static void get_target_resolution_dimensions(const BandData* bands, bool target_resolution_10m,
                                             int* width_out, int* height_out)
{
    if (target_resolution_10m)
    {
        *width_out = *bands[B04].width;
        *height_out = *bands[B04].height;
    }
    else
    {
        *width_out = *bands[B11].width;
        *height_out = *bands[B11].height;
    }
}

static void free_band_data(BandData bands[4])
{
    for (int i = 0; i < 4; i++)
    {
        if (*(bands[i].processed_data))
        {
            free(*(bands[i].processed_data));
        }
        else if (*(bands[i].raw_data))
        {
            free(*(bands[i].raw_data));
        }
        *(bands[i].raw_data) = NULL;
        *(bands[i].processed_data) = NULL;
    }
}

// Zwraca 0 dla sukcesu i -1 dla błędu
static int process_bands_and_calculate_indices(BandData bands[4], bool res_10m_selected,
                                               IndexMapData** out_map_data)
{
    float* ndvi_result = NULL;
    float* ndmi_result = NULL;
    IndexMapData* map_display_data = NULL;
    int final_processing_width, final_processing_height;

    // Load all bands data
    if (load_all_bands_data(bands) != 0)
    {
        return -1;
    }

    // Get target resolution dimensions
    get_target_resolution_dimensions(bands, res_10m_selected,
                                     &final_processing_width, &final_processing_height);

    // Resample bands to target resolution
    if (resample_all_bands_to_target_resolution(bands, 4, res_10m_selected) != 0)
    {
        free_band_data(bands);
        return -1;
    }

    // Calculate NDVI
    ndvi_result = calculate_ndvi(*bands[B08].processed_data, *bands[B04].processed_data,
                                 final_processing_width, final_processing_height,
                                 *bands[SCL].processed_data);
    if (!ndvi_result)
    {
        g_printerr("[%s] Błąd podczas obliczania NDVI.\n", get_timestamp());
        free_band_data(bands);
        return -1;
    }

    // Calculate NDMI
    ndmi_result = calculate_ndmi(*bands[B08].processed_data, *bands[B11].processed_data,
                                 final_processing_width, final_processing_height,
                                 *bands[SCL].processed_data);
    if (!ndmi_result)
    {
        g_printerr("[%s] Błąd podczas obliczania NDMI.\n", get_timestamp());
        free(ndvi_result);
        free_band_data(bands);
        return -1;
    }

    // Free processed band data as we no longer need it
    free_band_data(bands);

    // Prepare display data
    map_display_data = g_new(IndexMapData, 1);
    if (!map_display_data)
    {
        g_printerr("[%s] Błąd alokacji pamięci dla IndexMapData.\n", get_timestamp());
        free(ndvi_result);
        free(ndmi_result);
        return -1;
    }

    map_display_data->ndvi_data = ndvi_result;
    map_display_data->ndmi_data = ndmi_result;
    map_display_data->width = final_processing_width;
    map_display_data->height = final_processing_height;
    strcpy(map_display_data->current_map_type, "NDVI");

    *out_map_data = map_display_data;
    return 0;
}

static void show_error_dialog(GtkWindow* parent_window, const char* message)
{
    if (GTK_IS_WINDOW(parent_window) && gtk_widget_get_visible(GTK_WIDGET(parent_window)))
    {
        GtkWidget* error_dialog = gtk_message_dialog_new(parent_window,
                                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                                         GTK_MESSAGE_ERROR,
                                                         GTK_BUTTONS_CLOSE,
                                                         "%s", message);
        gtk_window_set_title(GTK_WINDOW(error_dialog), "Błąd przetwarzania");
        gtk_dialog_run(GTK_DIALOG(error_dialog));
        gtk_widget_destroy(error_dialog);
    }
}

static void on_rozpocznij_clicked(GtkWidget* widget, gpointer user_data)
{
    GtkWidget* config_window_widget = GTK_WIDGET(user_data);
    GtkWindow* parent_gtk_window = GTK_WINDOW(user_data);

    int widths[4], heights[4];
    float* raw_data[4] = {NULL};
    float* processed_data[4] = {NULL};

    BandData bands[4] = {
        {&path_b04, &btn_load_b04_widget, &raw_data[0], &processed_data[0], &widths[0], &heights[0], "B04", 0},
        {&path_b08, &btn_load_b08_widget, &raw_data[1], &processed_data[1], &widths[1], &heights[1], "B08", 1},
        {&path_b11, &btn_load_b11_widget, &raw_data[2], &processed_data[2], &widths[2], &heights[2], "B11", 2},
        {&path_scl, &btn_load_scl_widget, &raw_data[3], &processed_data[3], &widths[3], &heights[3], "SCL", 3}
    };

    if (!validate_band_paths(bands, 4, parent_gtk_window))
    {
        return;
    }

    g_print("[%s] Rozpoczynanie wczytywania danych pasm.\n", get_timestamp());

    IndexMapData* map_display_data = NULL;
    int processing_result = process_bands_and_calculate_indices(bands, res_10m_selected, &map_display_data);

    if (processing_result != 0)
    {
        g_printerr("[%s] Wystąpił błąd podczas przetwarzania danych.\n", get_timestamp());
        show_error_dialog(parent_gtk_window, "Błąd podczas przetwarzania danych. Sprawdź konsolę.");
        return;
    }

    GtkApplication* app = gtk_window_get_application(GTK_WINDOW(config_window_widget));
    GtkWidget* map_window = create_map_window(app, map_display_data);

    if (map_window)
    {
        gtk_widget_show_all(map_window);
        gtk_widget_destroy(config_window_widget);
    }
    else
    {
        show_error_dialog(parent_gtk_window, "Błąd podczas tworzenia okna mapy.");
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

// === Funkcja cleanup (automatycznie wywoływana) ===
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
}

static GtkWidget* create_map_window(GtkApplication* app, IndexMapData* map_data)
{
    if (!map_data)
    {
        g_printerr("create_map_window_v3: map_data jest NULL.\n");
        return NULL;
    }

    if (!map_data->ndvi_data && !map_data->ndmi_data)
    {
        g_printerr("create_map_window_v3: Brak danych NDVI/NDMI.\n");
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


// Handler dla zniszczenia okna konfiguracji
static void on_config_window_destroy(GtkWidget* widget, gpointer data)
{
    g_print("on_config_window_destroy: Okno konfiguracji %p zniszczone. Ustawiam the_config_window na NULL.\n",
            (void*)widget);
    if (the_config_window == widget)
    {
        the_config_window = NULL;
    }
}

static void activate_config_window(GtkApplication* app, gpointer user_data)
{
    if (the_config_window)
    {
        if (gtk_widget_get_visible(the_config_window))
        {
            g_print("activate_config_window: Okno konfiguracji już istnieje i jest widoczne, prezentuję %p.\n",
                    (void*)the_config_window);
            gtk_window_present(GTK_WINDOW(the_config_window));
        }
        else
        {
            g_print("activate_config_window: Okno konfiguracji już istnieje, ale nie jest widoczne, pokazuję %p.\n",
                    (void*)the_config_window);
            gtk_widget_show_all(the_config_window);
            gtk_window_present(GTK_WINDOW(the_config_window));
        }
        return;
    }

    g_print("activate_config_window: Tworzenie nowego okna konfiguracji.\n");
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

static gboolean on_draw_map_area(GtkWidget* widget, cairo_t* cr, gpointer user_data)
{
    IndexMapData* map_data = (IndexMapData*)g_object_get_data(G_OBJECT(widget), "map_data_ptr");

    if (!map_data)
    {
        fprintf(stderr, "on_draw_map_area: map_data_ptr na drawing_area jest NULL. Rysowanie tła.\n");
        cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
        cairo_paint(cr);
        return TRUE;
    }

    const float* data_to_render = NULL;
    if (strcmp(map_data->current_map_type, "NDVI") == 0)
    {
        data_to_render = map_data->ndvi_data;
    }
    else if (strcmp(map_data->current_map_type, "NDMI") == 0)
    {
        data_to_render = map_data->ndmi_data;
    }

    if (!data_to_render)
    {
        fprintf(stderr, "on_draw_map_area: data_to_render jest NULL dla typu mapy %s.\n", map_data->current_map_type);
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
        fprintf(stderr, "on_draw_map_area: Nie udało się wygenerować pixbuf.\n");
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
