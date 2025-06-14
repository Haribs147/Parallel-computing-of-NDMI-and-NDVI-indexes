#include <gtk/gtk.h>
#include <gdal.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "../data_types/data_types.h"

#include "gui.h"
#include "../utils/gui_utils.h"
#include "../resampler/resampler.h"
#include "../utils/utils.h"
#include "../processing_pipeline/processing_pipeline.h"
#include "../visualization/visualization.h"
#include "../data_saver/data_saver.h"

#define DEFAULT_WINDOW_WIDTH 900
#define DEFAULT_WINDOW_HEIGHT 750

typedef struct
{
    GtkApplication* app;
    ProcessingResult* map_data;
    GdkPixbuf* ndvi_pixbuf;
    GdkPixbuf* ndmi_pixbuf;
} MapWindowData;

typedef struct
{
    const char* suffix;
    const char* prefix;
    const char* default_text;
} BandConfig;

// Dane pasma wyświetlane w GUI
const BandConfig band_configs[] = {
    {"pasma B04", "B04: ", "Wczytaj pasmo B04"},
    {"pasma B08", "B08: ", "Wczytaj pasmo B08"},
    {"pasma B11", "B11: ", "Wczytaj pasmo B11"},
    {"pasma SCL", "SCL: ", "Wczytaj pasmo SCL"},
    {NULL, "Plik: ", "Wybierz plik"} // default
};

// Zmienne globalne
static char* path_b04 = NULL;
static char* path_b08 = NULL;
static char* path_b11 = NULL;
static char* path_scl = NULL;
static gboolean res_10m_selected = TRUE;
char current_map_type[10] = "NDVI";

static char* save_filename_ndvi = NULL;
static char* save_filename_ndmi = NULL;
static gboolean save_results_to_file = FALSE;

static GtkWidget* btn_load_b04_widget = NULL;
static GtkWidget* btn_load_b08_widget = NULL;
static GtkWidget* btn_load_b11_widget = NULL;
static GtkWidget* btn_load_scl_widget = NULL;

static GtkWidget* entry_ndvi_filename_widget = NULL;
static GtkWidget* entry_ndmi_filename_widget = NULL;
static GtkWidget* check_save_results_widget = NULL;

// Statyczny wskaźnik do okna konfiguracji
static GtkWidget* the_config_window = NULL;

// ====== GUI - GŁÓWNE FUNKCJE ======
static void activate_config_window(GtkApplication* app);
static GtkWidget* create_map_window(GtkApplication* app, ProcessingResult* map_data);
static gboolean on_draw_map_area(GtkWidget* widget, cairo_t* cr, gpointer user_data);

// ====== GUI - OBSŁUGA ZDARZEŃ ======
static void on_config_window_destroy(GtkWidget* widget);
static void on_load_b04_clicked(GtkWidget* widget, gpointer data);
static void on_load_b08_clicked(GtkWidget* widget, gpointer data);
static void on_load_b11_clicked(GtkWidget* widget, gpointer data);
static void on_load_scl_clicked(GtkWidget* widget, gpointer data);
static void on_rozpocznij_clicked(GtkWidget* widget, gpointer user_data);
static void on_config_radio_resolution_toggled(GtkToggleButton* togglebutton);
static void on_map_type_radio_toggled(GtkToggleButton* togglebutton, gpointer user_data);
static void on_save_results_toggled(GtkToggleButton* toggle_button, gpointer user_data);

// ====== POMOCNICZE ======
const BandConfig* get_band_config(const gchar* dialog_title_suffix);
void update_file_selection(char** target_path_variable, GtkButton* button_to_update,
                           GtkFileChooser* file_chooser, const BandConfig* config);
void handle_file_selection(GtkWindow* parent_window, const gchar* dialog_title_suffix,
                           char** target_path_variable, GtkButton* button_to_update);

// ====== WALIDACJA ======
static int validate_band_paths(BandData bands[], int band_count, GtkWindow* parent_window);

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
    GtkWidget* save_options_grid;
    GtkWidget* label_ndvi_filename;
    GtkWidget* label_ndmi_filename;

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

    // Checkbox do włączenia zapisu
    check_save_results_widget = gtk_check_button_new_with_label("Zapisz wyniki do plików PNG");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_save_results_widget), save_results_to_file);
    g_signal_connect(check_save_results_widget, "toggled", G_CALLBACK(on_save_results_toggled), NULL);
    gtk_box_pack_start(GTK_BOX(main_vbox), check_save_results_widget, FALSE, FALSE, 5);

    // Użyjemy GtkGrid dla lepszego wyrównania etykiet i pól tekstowych
    save_options_grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(save_options_grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(save_options_grid), 5);
    gtk_box_pack_start(GTK_BOX(main_vbox), save_options_grid, FALSE, FALSE, 0);

    // Pole na nazwę pliku NDVI
    label_ndvi_filename = gtk_label_new("Nazwa pliku NDVI:");
    gtk_widget_set_halign(label_ndvi_filename, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(save_options_grid), label_ndvi_filename, 0, 0, 1, 1);

    entry_ndvi_filename_widget = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry_ndvi_filename_widget), "ndvi_output.png");
    gtk_widget_set_hexpand(entry_ndvi_filename_widget, TRUE);
    gtk_grid_attach(GTK_GRID(save_options_grid), entry_ndvi_filename_widget, 1, 0, 1, 1);

    // Pole na nazwę pliku NDMI
    label_ndmi_filename = gtk_label_new("Nazwa pliku NDMI:");
    gtk_widget_set_halign(label_ndmi_filename, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(save_options_grid), label_ndmi_filename, 0, 1, 1, 1);

    entry_ndmi_filename_widget = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry_ndmi_filename_widget), "ndmi_output.png");
    gtk_widget_set_hexpand(entry_ndmi_filename_widget, TRUE);
    gtk_grid_attach(GTK_GRID(save_options_grid), entry_ndmi_filename_widget, 1, 1, 1, 1);

    // Ustawienie początkowej czułości pól tekstowych
    gtk_widget_set_sensitive(entry_ndvi_filename_widget, save_results_to_file);
    gtk_widget_set_sensitive(entry_ndmi_filename_widget, save_results_to_file);

    btn_rozpocznij = gtk_button_new_with_label("Rozpocznij");
    g_signal_connect(btn_rozpocznij, "clicked", G_CALLBACK(on_rozpocznij_clicked), config_window_local);
    gtk_box_pack_start(GTK_BOX(main_vbox), btn_rozpocznij, FALSE, FALSE, 0);
    gtk_widget_set_halign(btn_rozpocznij, GTK_ALIGN_START);

    gtk_widget_show_all(config_window_local);
}

static GtkWidget* create_map_window(GtkApplication* app, ProcessingResult* map_data)
{
    // Tworzenie okna
    GtkWidget* map_window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(map_window), "Wynikowa Mapa Wskaźników");
    gtk_window_set_default_size(GTK_WINDOW(map_window), DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT);
    gtk_container_set_border_width(GTK_CONTAINER(map_window), 10);

    // Enkapsulacja danych okna z automatycznym cleanup
    MapWindowData* window_data = g_new0(MapWindowData, 1);
    window_data->app = app;
    window_data->map_data = map_data;

    if (map_data && map_data->ndvi_data)
    {
        window_data->ndvi_pixbuf = generate_pixbuf_from_index_data(map_data->ndvi_data, map_data->width,
                                                                   map_data->height);
    }
    if (map_data && map_data->ndmi_data)
    {
        window_data->ndmi_pixbuf = generate_pixbuf_from_index_data(map_data->ndmi_data, map_data->width,
                                                                   map_data->height);
    }

    if (!window_data->ndvi_pixbuf || !window_data->ndmi_pixbuf)
    {
        g_printerr("[%s] Nie udało się wygenerować jednego lub obu pixbufów dla map.\n", get_timestamp());
        if (window_data->ndvi_pixbuf) g_object_unref(window_data->ndvi_pixbuf);
        if (window_data->ndmi_pixbuf) g_object_unref(window_data->ndmi_pixbuf);
        g_free(window_data);
        gtk_widget_destroy(map_window);
        return NULL;
    }

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
    g_signal_connect(drawing_area, "draw", G_CALLBACK(on_draw_map_area), window_data);

    // Radio buttons
    GtkWidget* radio_ndmi = gtk_radio_button_new_with_label(NULL, "NDMI");
    gtk_box_pack_start(GTK_BOX(radio_hbox), radio_ndmi, FALSE, FALSE, 0);
    g_signal_connect(radio_ndmi, "toggled", G_CALLBACK(on_map_type_radio_toggled), drawing_area);

    GtkWidget* radio_ndvi = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio_ndmi), "NDVI");
    gtk_box_pack_start(GTK_BOX(radio_hbox), radio_ndvi, FALSE, FALSE, 0);
    g_signal_connect(radio_ndvi, "toggled", G_CALLBACK(on_map_type_radio_toggled), drawing_area);

    // Ustawienie domyślnego wyboru
    if (strcmp(current_map_type, "NDVI") == 0)
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

static gboolean on_draw_map_area(GtkWidget* widget, cairo_t* cr, gpointer user_data)
{
    MapWindowData* win_data = (MapWindowData*)user_data;

    if (!win_data || !win_data->map_data)
    {
        g_printerr("[%s] Błąd krytyczny w on_draw_map_area: brak danych okna lub mapy.\n", get_timestamp());
        cairo_set_source_rgb(cr, 0.5, 0.0, 0.5);
        cairo_paint(cr);
        return TRUE;
    }

    GdkPixbuf* pixbuf_to_draw = NULL;
    if (strcmp(current_map_type, "NDVI") == 0)
    {
        pixbuf_to_draw = win_data->ndvi_pixbuf;
    }
    else
    {
        pixbuf_to_draw = win_data->ndmi_pixbuf;
    }

    if (pixbuf_to_draw)
    {
        gdk_cairo_set_source_pixbuf(cr, pixbuf_to_draw, 0, 0);
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

    if (save_results_to_file)
    {
        const char* ndvi_filename_text = gtk_entry_get_text(GTK_ENTRY(entry_ndvi_filename_widget));
        const char* ndmi_filename_text = gtk_entry_get_text(GTK_ENTRY(entry_ndmi_filename_widget));

        if (strlen(ndvi_filename_text) == 0 || strlen(ndmi_filename_text) == 0)
        {
            show_error_dialog(parent_gtk_window,
                              "Jeśli zaznaczono opcję zapisu, nazwy plików NDVI i NDMI nie mogą być puste.");
            return;
        }
        // Zwolnij stare nazwy, jeśli istnieją
        if (save_filename_ndvi) g_free(save_filename_ndvi);
        save_filename_ndvi = g_strdup(ndvi_filename_text);

        if (save_filename_ndmi) g_free(save_filename_ndmi);
        save_filename_ndmi = g_strdup(ndmi_filename_text);
    }
    else
    {
        // Jeśli zapis nie jest włączony, upewnij się, że wskaźniki są NULL
        if (save_filename_ndvi) g_free(save_filename_ndvi);
        save_filename_ndvi = NULL;
        if (save_filename_ndmi) g_free(save_filename_ndmi);
        save_filename_ndmi = NULL;
    }

    // Przygotowanie danych pasm
    int widths[4], heights[4];
    float* raw_data[4] = {NULL};
    float* processed_data[4] = {NULL};

    BandData bands[4] = {
        {&path_b04, &raw_data[0], &processed_data[0], &widths[0], &heights[0], "B04"},
        {&path_b08, &raw_data[1], &processed_data[1], &widths[1], &heights[1], "B08"},
        {&path_b11, &raw_data[2], &processed_data[2], &widths[2], &heights[2], "B11"},
        {&path_scl, &raw_data[3], &processed_data[3], &widths[3], &heights[3], "SCL"}
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

    // Tworzenie okna mapy
    GtkApplication* app = gtk_window_get_application(GTK_WINDOW(config_window_widget));
    GtkWidget* map_window = create_map_window(app, processing_result);

    if (map_window)
    {
        if (save_results_to_file && save_filename_ndvi && save_filename_ndmi)
        {
            MapWindowData* window_data = g_object_get_data(G_OBJECT(map_window), "window_data");
            if (window_data)
            {
                save_pixbuf_to_png(window_data->ndvi_pixbuf, save_filename_ndvi);
                save_pixbuf_to_png(window_data->ndmi_pixbuf, save_filename_ndmi);
            }
            else
            {
                g_printerr("[%s] Nie można pobrać window_data do zapisu pixbufów.\n", get_timestamp());
            }
        }
        g_print("[%s] Pomyślnie utworzono okno mapy.\n", get_timestamp());
        gtk_widget_show_all(map_window);
        gtk_widget_destroy(config_window_widget);
    }
    else
    {
        g_printerr("[%s] Błąd podczas tworzenia okna mapy.\n", get_timestamp());
        show_error_dialog(parent_gtk_window, "Błąd podczas tworzenia okna mapy.");
        free_index_map_data(processing_result);
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
        const gchar* label = gtk_button_get_label(GTK_BUTTON(togglebutton));

        if (g_strcmp0(label, "NDVI") == 0)
        {
            strcpy(current_map_type, "NDVI");
        }
        else
        {
            strcpy(current_map_type, "NDMI");
        }

        if (GTK_IS_WIDGET(drawing_area) && gtk_widget_get_realized(drawing_area) &&
            gtk_widget_get_visible(drawing_area))
        {
            gtk_widget_queue_draw(drawing_area);
        }
    }
}

static void on_save_results_toggled(GtkToggleButton* toggle_button, gpointer user_data)
{
    save_results_to_file = gtk_toggle_button_get_active(toggle_button);

    // Włącz lub wyłącz pola tekstowe nazw plików w zależności od stanu checkboxa
    gtk_widget_set_sensitive(entry_ndvi_filename_widget, save_results_to_file);
    gtk_widget_set_sensitive(entry_ndmi_filename_widget, save_results_to_file);
}

// ====== IMPLEMENTACJE - POMOCNICZE ======

const BandConfig* get_band_config(const gchar* dialog_title_suffix)
{
    for (int i = 0; band_configs[i].suffix != NULL; i++)
    {
        if (g_strcmp0(dialog_title_suffix, band_configs[i].suffix) == 0)
        {
            return &band_configs[i];
        }
    }
    return &band_configs[4]; // default
}

void update_file_selection(char** target_path_variable, GtkButton* button_to_update,
                           GtkFileChooser* file_chooser, const BandConfig* config)
{
    char* filename_path = gtk_file_chooser_get_filename(file_chooser);
    if (!filename_path)
    {
        return; // Brak wybranego pliku
    }

    // Zwolnienie starej ścieżki
    if (*target_path_variable)
    {
        g_free(*target_path_variable);
    }
    *target_path_variable = g_strdup(filename_path);

    // Aktualizacja etykiety przycisku
    update_button_label(button_to_update, filename_path, config->prefix);

    g_free(filename_path);
}

void handle_file_selection(GtkWindow* parent_window, const gchar* dialog_title_suffix,
                           char** target_path_variable, GtkButton* button_to_update)
{
    const BandConfig* config = get_band_config(dialog_title_suffix);

    char dialog_title[150];
    snprintf(dialog_title, sizeof(dialog_title), "Wybierz plik dla %s", dialog_title_suffix);

    GtkWidget* dialog = create_file_dialog(parent_window, dialog_title);
    gint res = gtk_dialog_run(GTK_DIALOG(dialog));

    if (res == GTK_RESPONSE_ACCEPT)
    {
        update_file_selection(target_path_variable, button_to_update,
                              GTK_FILE_CHOOSER(dialog), config);
    }
    else if (!(*target_path_variable))
    {
        gtk_button_set_label(button_to_update, config->default_text);
    }

    gtk_widget_destroy(dialog);
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

// ====== IMPLEMENTACJE - PAMIĘĆ ======

static void free_index_map_data(gpointer data)
{
    g_print("[%s] Rozpoczęto zwalnianie danych mapy.\n", get_timestamp());
    if (data == NULL)
    {
        g_print("[%s] Mapa nie zawiera danych, kończenie.\n", get_timestamp());
        return;
    }

    ProcessingResult* map_data = data;

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
    MapWindowData* window_data = (MapWindowData*)data;
    if (!window_data)
    {
        g_print("[%s] window_data jest NULL, pomijam cleanup.\n", get_timestamp());
        return;
    }

    if (window_data->ndvi_pixbuf)
    {
        g_object_unref(window_data->ndvi_pixbuf);
        window_data->ndvi_pixbuf = NULL;
    }
    if (window_data->ndmi_pixbuf)
    {
        g_object_unref(window_data->ndmi_pixbuf);
        window_data->ndmi_pixbuf = NULL;
    }

    if (window_data->map_data)
    {
        free_index_map_data(window_data->map_data);
        window_data->map_data = NULL;
    }

    g_free(window_data);
    g_print("[%s] Zwalnianie danych zakończone.\n", get_timestamp());

    // Resetowanie ustawień domyślnych
    path_b04 = NULL;
    path_b08 = NULL;
    path_b11 = NULL;
    path_scl = NULL;
    res_10m_selected = TRUE;

    if (save_filename_ndvi)
    {
        g_free(save_filename_ndvi);
        save_filename_ndvi = NULL;
    }
    if (save_filename_ndmi)
    {
        g_free(save_filename_ndmi);
        save_filename_ndmi = NULL;
    }
}
