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
    IndexMapData* map_data_to_free;
} MapWindowCloseAndCleanupData;

// Deklaracje wprzód
static void create_and_show_map_window(GtkApplication* app, gpointer user_data);
static void activate_config_window(GtkApplication* app, gpointer user_data);
static gboolean on_draw_map_area(GtkWidget* widget, cairo_t* cr, gpointer user_data);
static gboolean on_map_window_delete_event_manual_cleanup(GtkWidget* widget, GdkEvent* event, gpointer user_data);
static void on_config_window_destroy(GtkWidget* widget, gpointer data);

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
    g_print("== free_index_map_data (WYWOŁANIE MANUALNE): Rozpoczęto zwalnianie danych mapy ==\n");
    if (data == NULL)
    {
        g_print("free_index_map_data: Wskaźnik 'data' jest NULL, kończenie.\n");
        return;
    }
    IndexMapData* map_data = (IndexMapData*)data;
    g_print("free_index_map_data: Wskaźnik map_data: %p\n", (void*)map_data);

    if (map_data->ndvi_data)
    {
        g_print("free_index_map_data: Zwalnianie ndvi_data (wskaźnik: %p)...\n", (void*)map_data->ndvi_data);
        free(map_data->ndvi_data);
        map_data->ndvi_data = NULL;
        g_print("free_index_map_data: Zwolniono ndvi_data.\n");
    }
    else
    {
        g_print("free_index_map_data: ndvi_data był NULL.\n");
    }

    if (map_data->ndmi_data)
    {
        g_print("free_index_map_data: Zwalnianie ndmi_data (wskaźnik: %p)...\n", (void*)map_data->ndmi_data);
        free(map_data->ndmi_data);
        map_data->ndmi_data = NULL;
        g_print("free_index_map_data: Zwolniono ndmi_data.\n");
    }
    else
    {
        g_print("free_index_map_data: ndmi_data był NULL.\n");
    }

    g_print("free_index_map_data: Zwalnianie struktury map_data (wskaźnik: %p) za pomocą g_free()...\n",
            (void*)map_data);
    g_free(map_data);
    g_print("== free_index_map_data: Zakończono zwalnianie danych mapy dla wskaźnika %p ==\n", data);
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
    for(int i = 0; i < band_count; i++) {
        if(!*(bands[i].path)) {
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
    for(int i = 0; i < 4; i++) {
        *(bands[i].raw_data) = LoadBandData(*(bands[i].path), bands[i].width, bands[i].height);
        if(!*(bands[i].raw_data)) {
            fprintf(stderr, "Błąd wczytywania %s!\n", bands[i].band_name);
            return -1;
        }
        *(bands[i].processed_data) = *(bands[i].raw_data);
    }
    return 0;
}

static void on_rozpocznij_clicked(GtkWidget* widget, gpointer user_data)
{
    GtkWidget* config_window_widget = GTK_WIDGET(user_data);
    GtkWindow* parent_gtk_window = GTK_WINDOW(user_data);

    // Inicjalizacja struktury danych pasm
    int widths[4], heights[4];
    float* raw_data[4] = {NULL};
    float* processed_data[4] = {NULL};

    BandData bands[4] = {
        {&path_b04, &btn_load_b04_widget, &raw_data[0], &processed_data[0], &widths[0], &heights[0], "B04", 0},
        {&path_b08, &btn_load_b08_widget, &raw_data[1], &processed_data[1], &widths[1], &heights[1], "B08", 1},
        {&path_b11, &btn_load_b11_widget, &raw_data[2], &processed_data[2], &widths[2], &heights[2], "B11", 2},
        {&path_scl, &btn_load_scl_widget, &raw_data[3], &processed_data[3], &widths[3], &heights[3], "SCL", 3}
    };

    float* ndvi_result = NULL;
    float* ndmi_result = NULL;
    IndexMapData* map_display_data = NULL;
    int final_processing_width;
    int final_processing_height;

    if (!validate_band_paths(bands, 4, parent_gtk_window)) {
        return;
    }

    g_print("[%s] Rozpoczynanie wczytywania danych pasm.\n", get_timestamp());

    if (load_all_bands_data(bands) != 0)
    {
        goto cleanup_processing_error;
    }


    // Określenie docelowej rozdzielczości
    if (res_10m_selected) {
        final_processing_width = *bands[B04].width;
        final_processing_height = *bands[B04].height;
    } else {
        final_processing_width = *bands[B11].width;
        final_processing_height = *bands[B11].height;
    }

    // Resampling pasm do docelowej rozdzielczości
    for(int i = 0; i < 4; i++) {
        if(*bands[i].width != final_processing_width || *bands[i].height != final_processing_height) {
            float* resampled = NULL;

            if(res_10m_selected) {
                // Upsampling B11 i SCL do 10m
                if(i == B11) { // B11 - bilinear interpolation
                    resampled = bilinear_resample_float(*(bands[i].raw_data), *bands[i].width, *bands[i].height,
                                                        final_processing_width, final_processing_height);
                    if(!resampled) {
                        fprintf(stderr, "Błąd upsamplingu %s.\n", bands[i].band_name);
                        goto cleanup_processing_error;
                    }
                } else if(i == SCL) { // SCL - nearest neighbor
                    resampled = nearest_neighbor_resample_scl(*(bands[i].raw_data), *bands[i].width, *bands[i].height,
                                                              final_processing_width, final_processing_height);
                    if(!resampled) {
                        fprintf(stderr, "Błąd upsamplingu %s.\n", bands[i].band_name);
                        goto cleanup_processing_error;
                    }
                }
            } else {
                // Downsampling B04 i B08 do 20m
                if(i == B04 || i == B08) { // B04 lub B08 - averaging
                    resampled = average_resample_float(*(bands[i].raw_data), *bands[i].width, *bands[i].height,
                                                       final_processing_width, final_processing_height);
                    if(!resampled) {
                        fprintf(stderr, "Błąd downsamplingu %s.\n", bands[i].band_name);
                        goto cleanup_processing_error;
                    }
                }
            }

            // Zamiana danych na nowe po resamplingu
            if(resampled) {
                if(*(bands[i].processed_data) == *(bands[i].raw_data)) {
                    free(*(bands[i].raw_data));
                } else {
                    free(*(bands[i].processed_data));
                }
                *(bands[i].raw_data) = NULL;
                *(bands[i].processed_data) = resampled;
            }
        }
    }

    // Obliczanie wskaźników NDVI i NDMI
    ndvi_result = calculate_ndvi(*bands[B08].processed_data, *bands[B04].processed_data,
                                 final_processing_width, final_processing_height,
                                 *bands[SCL].processed_data);
    if (!ndvi_result) {
        fprintf(stderr, "Błąd podczas obliczania NDVI.\n");
        goto cleanup_processing_error;
    }

    ndmi_result = calculate_ndmi(*bands[B08].processed_data, *bands[B11].processed_data,
                                 final_processing_width, final_processing_height,
                                 *bands[SCL].processed_data);
    if (!ndmi_result) {
        fprintf(stderr, "Błąd podczas obliczania NDMI.\n");
        goto cleanup_processing_error;
    }

    // Zwolnienie pamięci po przetworzonych danych pasm
    for(int i = 0; i < 4; i++) {
        if(*(bands[i].processed_data)) {
            free(*(bands[i].processed_data));
            *(bands[i].processed_data) = NULL;
        }
    }

    // Przygotowanie danych do wyświetlenia
    map_display_data = g_new(IndexMapData, 1);
    if (!map_display_data) {
        fprintf(stderr, "Błąd alokacji pamięci dla IndexMapData.\n");
        goto cleanup_processing_error;
    }

    map_display_data->ndvi_data = ndvi_result;
    ndvi_result = NULL;
    map_display_data->ndmi_data = ndmi_result;
    ndmi_result = NULL;
    map_display_data->width = final_processing_width;
    map_display_data->height = final_processing_height;
    strcpy(map_display_data->current_map_type, "NDVI");

    // Utworzenie okna mapy
    GtkApplication* app = gtk_window_get_application(GTK_WINDOW(config_window_widget));
    create_and_show_map_window(app, map_display_data);
    map_display_data = NULL;

    // Zniszczenie okna konfiguracji
    if (config_window_widget == the_config_window) {
        g_print("Niszczenie starego okna konfiguracji po utworzeniu mapy.\n");
        gtk_widget_destroy(config_window_widget);
    } else {
        gtk_widget_destroy(config_window_widget);
    }
    return;

cleanup_processing_error:
    g_print("Wystąpił błąd w on_rozpocznij_clicked, zwalnianie zasobów...\n");

    // Zwolnienie wyników obliczeń
    if (ndvi_result) {
        free(ndvi_result);
        ndvi_result = NULL;
    }
    if (ndmi_result) {
        free(ndmi_result);
        ndmi_result = NULL;
    }
    if (map_display_data) {
        free_index_map_data(map_display_data);
        map_display_data = NULL;
    }

    // Zwolnienie danych pasm
    for(int i = 0; i < 4; i++) {
        if(*(bands[i].processed_data)) {
            free(*(bands[i].processed_data));
        } else if(*(bands[i].raw_data)) {
            free(*(bands[i].raw_data));
        }
        *(bands[i].raw_data) = NULL;
        *(bands[i].processed_data) = NULL;
    }

    // Wyświetlenie błędu użytkownikowi
    if (GTK_IS_WINDOW(parent_gtk_window) && gtk_widget_get_visible(GTK_WIDGET(parent_gtk_window))) {
        GtkWidget* processing_error_dialog = gtk_message_dialog_new(parent_gtk_window, GTK_DIALOG_DESTROY_WITH_PARENT,
                                                                    GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                                                                    "Błąd podczas przetwarzania danych. Sprawdź konsolę.");
        gtk_window_set_title(GTK_WINDOW(processing_error_dialog), "Błąd przetwarzania");
        gtk_dialog_run(GTK_DIALOG(processing_error_dialog));
        gtk_widget_destroy(processing_error_dialog);
    }
}

static void on_config_radio_resolution_toggled(GtkToggleButton* togglebutton, gpointer data)
{
    if (gtk_toggle_button_get_active(togglebutton))
    {
        const gchar* label = gtk_button_get_label(GTK_BUTTON(togglebutton));
        if (g_str_has_prefix(label, "upscaling")) { res_10m_selected = TRUE; }
        else if (g_str_has_prefix(label, "downscaling")) { res_10m_selected = FALSE; }
    }
}

static void on_map_type_radio_toggled(GtkToggleButton* togglebutton, gpointer user_data)
{
    if (gtk_toggle_button_get_active(togglebutton))
    {
        GtkWidget* drawing_area = GTK_WIDGET(user_data);
        IndexMapData* map_data = (IndexMapData*)g_object_get_data(G_OBJECT(drawing_area), "map_data_ptr");
        if (!map_data)
        {
            g_warning("on_map_type_radio_toggled: map_data jest NULL dla drawing_area.");
            return;
        }
        const gchar* label = gtk_button_get_label(GTK_BUTTON(togglebutton));
        if (g_strcmp0(label, "NDVI") == 0) { strcpy(map_data->current_map_type, "NDVI"); }
        else if (g_strcmp0(label, "NDMI") == 0) { strcpy(map_data->current_map_type, "NDMI"); }

        if (GTK_IS_WIDGET(drawing_area) && gtk_widget_get_realized(drawing_area) &&
            gtk_widget_get_visible(drawing_area))
        {
            gtk_widget_queue_draw(drawing_area);
        }
    }
}

static gboolean on_map_window_delete_event_manual_cleanup(GtkWidget* widget, GdkEvent* event, gpointer user_data)
{
    g_print("== on_map_window_delete_event_manual_cleanup: Rozpoczęto obsługę zamknięcia okna mapy przez 'x' ==\n");
    MapWindowCloseAndCleanupData* cleanup_data = (MapWindowCloseAndCleanupData*)user_data;

    if (!cleanup_data)
    {
        g_warning("on_map_window_delete_event_manual_cleanup: cleanup_data jest NULL!");
        return TRUE;
    }

    // 1. Upewnij się, że drawing_area nie będzie już próbowało używać map_data.
    GtkWidget* toplevel = gtk_widget_get_toplevel(widget);
    if (GTK_IS_WINDOW(toplevel))
    {
        GtkWidget* main_vbox = gtk_bin_get_child(GTK_BIN(toplevel));
        if (main_vbox && GTK_IS_BOX(main_vbox))
        {
            GList* children = gtk_container_get_children(GTK_CONTAINER(main_vbox));
            for (GList* iter = children; iter != NULL; iter = g_list_next(iter))
            {
                GtkWidget* child_widget = GTK_WIDGET(iter->data);
                if (GTK_IS_SCROLLED_WINDOW(child_widget))
                {
                    GtkWidget* drawing_area_map = gtk_bin_get_child(GTK_BIN(child_widget));
                    if (drawing_area_map && GTK_IS_DRAWING_AREA(drawing_area_map))
                    {
                        g_print("on_map_window_delete_event_manual_cleanup: Usuwanie map_data_ptr z drawing_area %p\n",
                                (void*)drawing_area_map);
                        g_object_set_data(G_OBJECT(drawing_area_map), "map_data_ptr", NULL);
                        break;
                    }
                }
            }
            g_list_free(children);
        }
    }

    // 2. Zwolnij dane mapy
    if (cleanup_data->map_data_to_free)
    {
        g_print("on_map_window_delete_event_manual_cleanup: Wywoływanie free_index_map_data dla %p.\n",
                (void*)cleanup_data->map_data_to_free);
        free_index_map_data(cleanup_data->map_data_to_free);
        cleanup_data->map_data_to_free = NULL;
    }
    else
    {
        g_print("on_map_window_delete_event_manual_cleanup: map_data_to_free był już NULL.\n");
    }

    // 3. Reaktywuj aplikację (aby pokazać okno konfiguracji)
    if (cleanup_data->app)
    {
        g_print("on_map_window_delete_event_manual_cleanup: Ponowna aktywacja aplikacji.\n");
        g_application_activate(G_APPLICATION(cleanup_data->app));
    }

    // 4. Odłącz ten handler od sygnału
    g_print("on_map_window_delete_event_manual_cleanup: Odłączanie handlera od okna %p.\n", (void*)widget);
    g_signal_handlers_disconnect_by_func(widget,
                                         on_map_window_delete_event_manual_cleanup,
                                         cleanup_data);

    // 5. ODROCZONE zniszczenie okna mapy
    g_print("on_map_window_delete_event_manual_cleanup: Kolejkowanie zniszczenia okna mapy %p przez g_idle_add.\n",
            (void*)widget);
    g_object_ref(widget); // Dodaj referencję, bo widget będzie użyty w idle callback
    g_idle_add(destroy_widget_idle, widget);

    // 6. Zwolnij strukturę MapWindowCloseAndCleanupData
    g_print("on_map_window_delete_event_manual_cleanup: Zwalnianie struktury cleanup_data %p.\n", (void*)cleanup_data);
    g_free(cleanup_data);

    g_print("== on_map_window_delete_event_manual_cleanup: Zakończono (zniszczenie okna odroczone). ==\n");
    return TRUE; // Zdarzenie w pełni obsłużone
}

static void create_and_show_map_window(GtkApplication* app, gpointer user_data)
{
    IndexMapData* passed_map_data = (IndexMapData*)user_data;

    if (passed_map_data == NULL)
    {
        fprintf(stderr, "create_and_show_map_window: Otrzymano NULL jako user_data.\n");
        return;
    }
    if (!passed_map_data->ndvi_data && !passed_map_data->ndmi_data)
    {
        fprintf(stderr, "create_and_show_map_window: Krytyczny błąd - dane NDVI lub NDMI w map_data są NULL.\n");
        free_index_map_data(passed_map_data);
        return;
    }

    GtkWidget* map_window;
    GtkWidget* map_main_vbox;
    GtkWidget* radio_map_hbox;
    GtkWidget *radio_ndmi, *radio_ndvi;
    GtkWidget* scrolled_window_for_map;
    GtkWidget* drawing_area_map;

    map_window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(map_window), "Wynikowa Mapa Wskaźników");
    gtk_window_set_default_size(GTK_WINDOW(map_window), DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT);
    gtk_container_set_border_width(GTK_CONTAINER(map_window), 10);

    MapWindowCloseAndCleanupData* cleanup_data_for_event = g_new(MapWindowCloseAndCleanupData, 1);
    cleanup_data_for_event->app = app;
    cleanup_data_for_event->map_data_to_free = passed_map_data;

    g_print("create_and_show_map_window: Podłączanie on_map_window_delete_event_manual_cleanup do 'delete-event'.\n");
    g_print("  map_data_to_free: %p\n", (void*)passed_map_data);
    g_signal_connect(map_window, "delete-event", G_CALLBACK(on_map_window_delete_event_manual_cleanup),
                     cleanup_data_for_event);

    map_main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(map_window), map_main_vbox);

    radio_map_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(radio_map_hbox, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(map_main_vbox), radio_map_hbox, FALSE, FALSE, 0);

    drawing_area_map = gtk_drawing_area_new();
    g_object_set_data(G_OBJECT(drawing_area_map), "map_data_ptr", passed_map_data);

    radio_ndmi = gtk_radio_button_new_with_label(NULL, "NDMI");
    gtk_box_pack_start(GTK_BOX(radio_map_hbox), radio_ndmi, FALSE, FALSE, 0);
    g_signal_connect(radio_ndmi, "toggled", G_CALLBACK(on_map_type_radio_toggled), drawing_area_map);

    radio_ndvi = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio_ndmi), "NDVI");
    gtk_box_pack_start(GTK_BOX(radio_map_hbox), radio_ndvi, FALSE, FALSE, 0);
    g_signal_connect(radio_ndvi, "toggled", G_CALLBACK(on_map_type_radio_toggled), drawing_area_map);

    if (strcmp(passed_map_data->current_map_type, "NDVI") == 0)
    {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_ndvi), TRUE);
    }
    else
    {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_ndmi), TRUE);
    }

    scrolled_window_for_map = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(scrolled_window_for_map), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand(scrolled_window_for_map, TRUE);
    gtk_widget_set_vexpand(scrolled_window_for_map, TRUE);
    gtk_box_pack_start(GTK_BOX(map_main_vbox), scrolled_window_for_map, TRUE, TRUE, 0);

    gtk_widget_set_size_request(drawing_area_map, passed_map_data->width, passed_map_data->height);
    g_signal_connect(drawing_area_map, "draw", G_CALLBACK(on_draw_map_area), passed_map_data);
    gtk_container_add(GTK_CONTAINER(scrolled_window_for_map), drawing_area_map);

    gtk_widget_show_all(map_window);
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