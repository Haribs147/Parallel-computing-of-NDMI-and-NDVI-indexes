#include <gtk/gtk.h>
#include <gdal.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <string.h>
#include <pango/pango.h>

#include "gui.h"
#include "data_loader.h"
#include "resampler.h"
#include "utils.h"
#include "index_calculator.h"
#include "visualization.h"

#define DEFAULT_WINDOW_WIDTH 900
#define DEFAULT_WINDOW_HEIGHT 750

typedef struct {
    float* ndvi_data;
    float* ndmi_data;
    int width;
    int height;
    char current_map_type[10];
} IndexMapData;

// ZMODYFIKOWANA struktura dla handlera "delete-event"
typedef struct {
    GtkApplication *app;
    IndexMapData *map_data_to_free; // Dane do zwolnienia przez handler
    // map_window_widget usunięte, użyjemy argumentu 'widget' z callbacku
} MapWindowCloseAndCleanupData;

// Deklaracje wprzód
static void create_and_show_map_window(GtkApplication *app, gpointer user_data);
static void activate_config_window(GtkApplication *app, gpointer user_data);
static gboolean on_draw_map_area(GtkWidget *widget, cairo_t *cr, gpointer user_data);
static gboolean on_map_window_delete_event_manual_cleanup(GtkWidget *widget, GdkEvent *event, gpointer user_data);
static void on_config_window_destroy(GtkWidget *widget, gpointer data); // Nowa
static gboolean destroy_widget_idle(gpointer data); // Nowa

static char *path_b04 = NULL;
static char *path_b08 = NULL;
static char *path_b11 = NULL;
static char *path_scl = NULL;
static gboolean res_10m_selected = TRUE;

static GtkWidget *btn_load_b04_widget = NULL;
static GtkWidget *btn_load_b08_widget = NULL;
static GtkWidget *btn_load_b11_widget = NULL;
static GtkWidget *btn_load_scl_widget = NULL;

// Statyczny wskaźnik do okna konfiguracji
static GtkWidget *the_config_window = NULL;


static void free_index_map_data(gpointer data) {
    g_print("== free_index_map_data (WYWOŁANIE MANUALNE): Rozpoczęto zwalnianie danych mapy ==\n");
    if (data == NULL) {
        g_print("free_index_map_data: Wskaźnik 'data' jest NULL, kończenie.\n");
        return;
    }
    IndexMapData *map_data = (IndexMapData*)data;
    g_print("free_index_map_data: Wskaźnik map_data: %p\n", (void*)map_data);

    if (map_data->ndvi_data) {
        g_print("free_index_map_data: Zwalnianie ndvi_data (wskaźnik: %p)...\n", (void*)map_data->ndvi_data);
        free(map_data->ndvi_data);
        map_data->ndvi_data = NULL;
        g_print("free_index_map_data: Zwolniono ndvi_data.\n");
    } else {
        g_print("free_index_map_data: ndvi_data był NULL.\n");
    }

    if (map_data->ndmi_data) {
        g_print("free_index_map_data: Zwalnianie ndmi_data (wskaźnik: %p)...\n", (void*)map_data->ndmi_data);
        free(map_data->ndmi_data);
        map_data->ndmi_data = NULL;
        g_print("free_index_map_data: Zwolniono ndmi_data.\n");
    } else {
        g_print("free_index_map_data: ndmi_data był NULL.\n");
    }

    g_print("free_index_map_data: Zwalnianie struktury map_data (wskaźnik: %p) za pomocą g_free()...\n", (void*)map_data);
    g_free(map_data);
    g_print("== free_index_map_data: Zakończono zwalnianie danych mapy dla wskaźnika %p ==\n", data);
}

static void handle_file_selection(GtkWindow *parent_window, const gchar *dialog_title_suffix, char **target_path_variable, GtkButton *button_to_update) {
    GtkWidget *dialog;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
    gint res;
    char dialog_full_title[100];
    char button_label_prefix[50];
    char default_button_text[100];

    if (g_strcmp0(dialog_title_suffix, "pasma B04") == 0) {
        snprintf(button_label_prefix, sizeof(button_label_prefix), "B04: ");
        snprintf(default_button_text, sizeof(default_button_text), "Wczytaj pasmo B04");
    } else if (g_strcmp0(dialog_title_suffix, "pasma B08") == 0) {
        snprintf(button_label_prefix, sizeof(button_label_prefix), "B08: ");
        snprintf(default_button_text, sizeof(default_button_text), "Wczytaj pasmo B08");
    } else if (g_strcmp0(dialog_title_suffix, "pasma B11") == 0) {
        snprintf(button_label_prefix, sizeof(button_label_prefix), "B11: ");
        snprintf(default_button_text, sizeof(default_button_text), "Wczytaj pasmo B11");
    } else if (g_strcmp0(dialog_title_suffix, "pasma SCL") == 0) {
        snprintf(button_label_prefix, sizeof(button_label_prefix), "SCL: ");
        snprintf(default_button_text, sizeof(default_button_text), "Wczytaj pasmo SCL");
    } else {
        snprintf(button_label_prefix, sizeof(button_label_prefix), "Plik: ");
        snprintf(default_button_text, sizeof(default_button_text), "Wybierz plik");
    }

    snprintf(dialog_full_title, sizeof(dialog_full_title), "Wybierz plik dla %s", dialog_title_suffix);

    dialog = gtk_file_chooser_dialog_new(dialog_full_title, parent_window, action,
                                         "_Anuluj", GTK_RESPONSE_CANCEL,
                                         "_Otwórz", GTK_RESPONSE_ACCEPT, NULL);

    GtkFileFilter *filter_jp2 = gtk_file_filter_new();
    gtk_file_filter_set_name(filter_jp2, "Obrazy JP2 (*.jp2)");
    gtk_file_filter_add_pattern(filter_jp2, "*.jp2");
    gtk_file_filter_add_pattern(filter_jp2, "*.JP2");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter_jp2);

    res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        char *filename_path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (*target_path_variable) {
            g_free(*target_path_variable);
        }
        *target_path_variable = g_strdup(filename_path);
        // g_print("Wybrano plik dla %s: %s\n", dialog_title_suffix, filename_path); // Mniej gadatliwe
        const char *short_name = get_short_filename(filename_path);
        char new_label[256];
        snprintf(new_label, sizeof(new_label), "%s%s", button_label_prefix, short_name);
        gtk_button_set_label(button_to_update, new_label);
        GList *children = gtk_container_get_children(GTK_CONTAINER(button_to_update));
        if (children != NULL) {
            GtkWidget *label_widget = GTK_WIDGET(children->data);
            if (GTK_IS_LABEL(label_widget)) {
                gtk_label_set_ellipsize(GTK_LABEL(label_widget), PANGO_ELLIPSIZE_END);
            }
            g_list_free(children);
        }
        g_free(filename_path);
    } else {
        if (button_to_update && !(*target_path_variable)) {
            gtk_button_set_label(button_to_update, default_button_text);
        }
    }
    gtk_widget_destroy(dialog);
}

static void on_load_b04_clicked(GtkWidget *widget, gpointer data) { handle_file_selection(GTK_WINDOW(data), "pasma B04", &path_b04, GTK_BUTTON(widget)); }
static void on_load_b08_clicked(GtkWidget *widget, gpointer data) { handle_file_selection(GTK_WINDOW(data), "pasma B08", &path_b08, GTK_BUTTON(widget)); }
static void on_load_b11_clicked(GtkWidget *widget, gpointer data) { handle_file_selection(GTK_WINDOW(data), "pasma B11", &path_b11, GTK_BUTTON(widget)); }
static void on_load_scl_clicked(GtkWidget *widget, gpointer data) { handle_file_selection(GTK_WINDOW(data), "pasma SCL", &path_scl, GTK_BUTTON(widget)); }


static void on_rozpocznij_clicked(GtkWidget *widget, gpointer user_data) {
    GtkWidget *config_window_widget = GTK_WIDGET(user_data);
    GtkWindow *parent_gtk_window = GTK_WINDOW(user_data);

    g_print("Przycisk 'Rozpocznij' kliknięty.\n");

    if (!path_b04 || !path_b08 || !path_b11 || !path_scl) {
        GtkWidget *error_dialog = gtk_message_dialog_new(parent_gtk_window, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "Błąd: Nie wszystkie pasma zostały wybrane!\n\nProszę wybrać pliki dla pasm B04, B08, B11 oraz SCL.");
        gtk_window_set_title(GTK_WINDOW(error_dialog), "Błąd wyboru plików");
        gtk_dialog_run(GTK_DIALOG(error_dialog));
        gtk_widget_destroy(error_dialog);
        return;
    }

    g_print("\nRozpoczynanie wczytywania danych pasm...\n");
    float* pafBand4Data = NULL; int nBand4Width = 0, nBand4Height = 0;
    float* pafBand8Data = NULL; int nBand8Width = 0, nBand8Height = 0;
    float* pafBand11Data = NULL; int nBand11Width = 0, nBand11Height = 0;
    float* pafSCLData = NULL; int nSCLWidth = 0, nSCLHeight = 0;

    float* pafBand4Processed = NULL;
    float* pafBand8Processed = NULL;
    float* pafBand11Processed = NULL;
    float* pafSCLProcessed = NULL;

    float* ndvi_result = NULL;
    float* ndmi_result = NULL;
    IndexMapData *map_display_data = NULL;

    int final_processing_width;
    int final_processing_height;

    pafBand4Data = LoadBandData(path_b04, &nBand4Width, &nBand4Height);
    if(!pafBand4Data) { fprintf(stderr, "Błąd wczytywania B04!\n"); goto cleanup_processing_error; }
    pafBand4Processed = pafBand4Data;

    pafBand8Data = LoadBandData(path_b08, &nBand8Width, &nBand8Height);
    if(!pafBand8Data) { fprintf(stderr, "Błąd wczytywania B08!\n"); goto cleanup_processing_error; }
    pafBand8Processed = pafBand8Data;

    pafBand11Data = LoadBandData(path_b11, &nBand11Width, &nBand11Height);
    if(!pafBand11Data) { fprintf(stderr, "Błąd wczytywania B11!\n"); goto cleanup_processing_error; }
    pafBand11Processed = pafBand11Data;

    pafSCLData = LoadBandData(path_scl, &nSCLWidth, &nSCLHeight);
    if(!pafSCLData) { fprintf(stderr, "Błąd wczytywania SCL!\n"); goto cleanup_processing_error; }
    pafSCLProcessed = pafSCLData;

    // g_print("Wszystkie wymagane pasma wczytane pomyślnie.\n"); // Mniej gadatliwe

    if (res_10m_selected) {
        final_processing_width = nBand4Width;
        final_processing_height = nBand4Height;
        // g_print("Docelowa rozdzielczość: 10m (%dx%d)\n", final_processing_width, final_processing_height);

        if (nBand11Width != final_processing_width || nBand11Height != final_processing_height) {
            // g_print("Upsampling B11 do %dx%d...\n", final_processing_width, final_processing_height);
            float* resampled_b11 = bilinear_resample_float(pafBand11Data, nBand11Width, nBand11Height, final_processing_width, final_processing_height);
            if (resampled_b11) {
                if (pafBand11Processed == pafBand11Data) { free(pafBand11Data); } else { free(pafBand11Processed); }
                pafBand11Data = NULL;
                pafBand11Processed = resampled_b11;
                // g_print("Upsampling B11 zakończony.\n");
            } else { fprintf(stderr, "Błąd upsamplingu B11.\n"); goto cleanup_processing_error; }
        }
        if (nSCLWidth != final_processing_width || nSCLHeight != final_processing_height) {
            // g_print("Upsampling SCL do %dx%d...\n", final_processing_width, final_processing_height);
            float* resampled_scl = nearest_neighbor_resample_scl(pafSCLData, nSCLWidth, nSCLHeight, final_processing_width, final_processing_height);
            if (resampled_scl) {
                if (pafSCLProcessed == pafSCLData) { free(pafSCLData); } else { free(pafSCLProcessed); }
                pafSCLData = NULL;
                pafSCLProcessed = resampled_scl;
                // g_print("Upsampling SCL zakończony.\n");
            } else { fprintf(stderr, "Błąd upsamplingu SCL.\n"); goto cleanup_processing_error; }
        }
    } else {
        final_processing_width = nBand11Width;
        final_processing_height = nBand11Height;
        // g_print("Docelowa rozdzielczość: 20m (%dx%d)\n", final_processing_width, final_processing_height);
        if (nBand4Width != final_processing_width || nBand4Height != final_processing_height) {
            // g_print("Downsampling B04 do %dx%d...\n", final_processing_width, final_processing_height);
            float* resampled_b04 = average_resample_float(pafBand4Data, nBand4Width, nBand4Height, final_processing_width, final_processing_height);
            if (resampled_b04) {
                if (pafBand4Processed == pafBand4Data) { free(pafBand4Data); } else { free(pafBand4Processed); }
                pafBand4Data = NULL;
                pafBand4Processed = resampled_b04;
                // g_print("Downsampling B04 zakończony.\n");
            } else { fprintf(stderr, "Błąd downsamplingu B04.\n"); goto cleanup_processing_error; }
        }
        if (nBand8Width != final_processing_width || nBand8Height != final_processing_height) {
            // g_print("Downsampling B08 do %dx%d...\n", final_processing_width, final_processing_height);
            float* resampled_b08 = average_resample_float(pafBand8Data, nBand8Width, nBand8Height, final_processing_width, final_processing_height);
            if (resampled_b08) {
                if (pafBand8Processed == pafBand8Data) { free(pafBand8Data); } else { free(pafBand8Processed); }
                pafBand8Data = NULL;
                pafBand8Processed = resampled_b08;
                // g_print("Downsampling B08 zakończony.\n");
            } else { fprintf(stderr, "Błąd downsamplingu B08.\n"); goto cleanup_processing_error; }
        }
    }

    // g_print("\nObliczanie wskaźników NDVI i NDMI...\n");
    ndvi_result = calculate_ndvi(pafBand8Processed, pafBand4Processed,
                                 final_processing_width, final_processing_height,
                                 pafSCLProcessed);
    if (!ndvi_result) { fprintf(stderr, "Błąd podczas obliczania NDVI.\n"); goto cleanup_processing_error; }
    // printf("Obliczono NDVI (bufor: %p).\n", (void*)ndvi_result);

    ndmi_result = calculate_ndmi(pafBand8Processed, pafBand11Processed,
                                 final_processing_width, final_processing_height,
                                 pafSCLProcessed);
    if (!ndmi_result) { fprintf(stderr, "Błąd podczas obliczania NDMI.\n"); goto cleanup_processing_error; }
    // printf("Obliczono NDMI (bufor: %p).\n", (void*)ndmi_result);
    // g_print("Obliczanie wskaźników zakończone.\n");

    if (pafBand4Processed) { free(pafBand4Processed); pafBand4Processed = NULL; }
    if (pafBand8Processed) { free(pafBand8Processed); pafBand8Processed = NULL; }
    if (pafBand11Processed) { free(pafBand11Processed); pafBand11Processed = NULL; }
    if (pafSCLProcessed) { free(pafSCLProcessed); pafSCLProcessed = NULL; }
    // g_print("Zwolniono pamięć po przetworzonych pasmach (użytych do obliczenia wskaźników).\n");

    // g_print("Przygotowywanie danych do wyświetlenia...\n");
    map_display_data = g_new(IndexMapData, 1);
    if (!map_display_data) {
        fprintf(stderr, "Błąd alokacji pamięci dla IndexMapData.\n");
        goto cleanup_processing_error;
    }

    map_display_data->ndvi_data = ndvi_result; ndvi_result = NULL;
    map_display_data->ndmi_data = ndmi_result; ndmi_result = NULL;
    map_display_data->width = final_processing_width;
    map_display_data->height = final_processing_height;
    strcpy(map_display_data->current_map_type, "NDVI");

    GtkApplication *app = gtk_window_get_application(GTK_WINDOW(config_window_widget));
    create_and_show_map_window(app, map_display_data); map_display_data = NULL;

    // Nie niszczymy config_window_widget tutaj, jeśli the_config_window go śledzi.
    // Zamiast tego, on_map_window_delete_event_manual_cleanup aktywuje aplikację,
    // a activate_config_window pokaże istniejące the_config_window.
    // Jeśli config_window_widget ma być jednorazowe, to gtk_widget_destroy jest OK.
    // Dla tego przepływu, config_window jest niszczone, a nowe tworzone/pokazywane przez activate.
    if (config_window_widget == the_config_window) {
        // Jeśli to jest to samo okno, które śledzimy globalnie, nie niszczymy go tutaj,
        // bo chcemy do niego wrócić. Zostanie zniszczone, gdy użytkownik je zamknie.
        // Jednak activate_config_window i tak stworzy nowe, jeśli the_config_window jest NULL.
        // Aby uniknąć wielu okien konfiguracji, niszczymy to.
        g_print("Niszczenie starego okna konfiguracji po utworzeniu mapy.\n");
        gtk_widget_destroy(config_window_widget);
        // the_config_window zostanie ustawione na NULL przez on_config_window_destroy
    } else {
         gtk_widget_destroy(config_window_widget); // Jeśli to nie jest śledzone okno
    }
    return;

cleanup_processing_error:
    g_print("Wystąpił błąd w on_rozpocznij_clicked, zwalnianie zasobów...\n");
    if (ndvi_result) { free(ndvi_result); ndvi_result = NULL; }
    if (ndmi_result) { free(ndmi_result); ndmi_result = NULL; }
    if (map_display_data) { free_index_map_data(map_display_data); map_display_data = NULL; }

    if (pafBand4Processed) free(pafBand4Processed); else if(pafBand4Data) free(pafBand4Data);
    if (pafBand8Processed) free(pafBand8Processed); else if(pafBand8Data) free(pafBand8Data);
    if (pafBand11Processed) free(pafBand11Processed); else if(pafBand11Data) free(pafBand11Data);
    if (pafSCLProcessed) free(pafSCLProcessed); else if(pafSCLData) free(pafSCLData);
    pafBand4Data=pafBand8Data=pafBand11Data=pafSCLData=NULL;
    pafBand4Processed=pafBand8Processed=pafBand11Processed=pafSCLProcessed=NULL;

    if (GTK_IS_WINDOW(parent_gtk_window) && gtk_widget_get_visible(GTK_WIDGET(parent_gtk_window))) {
        GtkWidget *processing_error_dialog = gtk_message_dialog_new(parent_gtk_window, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "Błąd podczas przetwarzania danych. Sprawdź konsolę.");
        gtk_window_set_title(GTK_WINDOW(processing_error_dialog), "Błąd przetwarzania");
        gtk_dialog_run(GTK_DIALOG(processing_error_dialog));
        gtk_widget_destroy(processing_error_dialog);
    }
    return;
}

static void on_config_radio_resolution_toggled(GtkToggleButton *togglebutton, gpointer data) {
    if (gtk_toggle_button_get_active(togglebutton)) {
        const gchar *label = gtk_button_get_label(GTK_BUTTON(togglebutton));
        if (g_str_has_prefix(label, "upscaling")) { res_10m_selected = TRUE; }
        else if (g_str_has_prefix(label, "downscaling")) { res_10m_selected = FALSE; }
        // g_print("Wybrano rozdzielczość (konfiguracja): %s\n", res_10m_selected ? "10m" : "20m");
    }
}

static void on_map_type_radio_toggled(GtkToggleButton *togglebutton, gpointer user_data) {
    if (gtk_toggle_button_get_active(togglebutton)) {
        GtkWidget *drawing_area = GTK_WIDGET(user_data);
        IndexMapData *map_data = (IndexMapData*)g_object_get_data(G_OBJECT(drawing_area), "map_data_ptr");
        if (!map_data) {
            g_warning("on_map_type_radio_toggled: map_data jest NULL dla drawing_area.");
            return;
        }
        const gchar *label = gtk_button_get_label(GTK_BUTTON(togglebutton));
        if (g_strcmp0(label, "NDVI") == 0) { strcpy(map_data->current_map_type, "NDVI"); }
        else if (g_strcmp0(label, "NDMI") == 0) { strcpy(map_data->current_map_type, "NDMI"); }
        // g_print("Wybrano typ mapy do wyświetlenia: %s\n", map_data->current_map_type);
        if (GTK_IS_WIDGET(drawing_area) && gtk_widget_get_realized(drawing_area) && gtk_widget_get_visible(drawing_area)) {
            gtk_widget_queue_draw(drawing_area);
        }
    }
}

// Funkcja pomocnicza do odroczonego niszczenia widgetu
static gboolean destroy_widget_idle(gpointer data) {
    GtkWidget *widget_to_destroy = GTK_WIDGET(data);
    // Sprawdź, czy widget nadal istnieje i jest widgetem GTK
    // gtk_widget_in_destruction sprawdza, czy widget jest już w trakcie niszczenia
    if (widget_to_destroy && GTK_IS_WIDGET(widget_to_destroy) && !gtk_widget_in_destruction(widget_to_destroy)) {
        g_print("destroy_widget_idle: Niszczenie widgetu %p (odroczone).\n", (void*)widget_to_destroy);
        gtk_widget_destroy(widget_to_destroy);
    } else {
        g_print("destroy_widget_idle: Widget %p już zniszczony, w trakcie niszczenia lub nie jest widgetem. Nie niszczę ponownie.\n", (void*)widget_to_destroy);
    }
    g_object_unref(widget_to_destroy); // Zwolnij referencję dodaną dla g_idle_add
    return G_SOURCE_REMOVE; // Usuń źródło bezczynności po wykonaniu
}


static gboolean on_map_window_delete_event_manual_cleanup(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
    g_print("== on_map_window_delete_event_manual_cleanup: Rozpoczęto obsługę zamknięcia okna mapy przez 'x' ==\n");
    MapWindowCloseAndCleanupData *cleanup_data = (MapWindowCloseAndCleanupData*)user_data;

    if (!cleanup_data) {
        g_warning("on_map_window_delete_event_manual_cleanup: cleanup_data jest NULL!");
        return TRUE;
    }

    // 1. Upewnij się, że drawing_area nie będzie już próbowało używać map_data.
    GtkWidget *toplevel = gtk_widget_get_toplevel(widget); // widget to okno mapy
    if (GTK_IS_WINDOW(toplevel)) {
        GtkWidget *main_vbox = gtk_bin_get_child(GTK_BIN(toplevel));
        if (main_vbox && GTK_IS_BOX(main_vbox)) {
            GList *children = gtk_container_get_children(GTK_CONTAINER(main_vbox));
            for (GList *iter = children; iter != NULL; iter = g_list_next(iter)) {
                GtkWidget *child_widget = GTK_WIDGET(iter->data);
                if (GTK_IS_SCROLLED_WINDOW(child_widget)) {
                    GtkWidget *drawing_area_map = gtk_bin_get_child(GTK_BIN(child_widget));
                    if (drawing_area_map && GTK_IS_DRAWING_AREA(drawing_area_map)) {
                        g_print("on_map_window_delete_event_manual_cleanup: Usuwanie map_data_ptr z drawing_area %p\n", (void*)drawing_area_map);
                        g_object_set_data(G_OBJECT(drawing_area_map), "map_data_ptr", NULL);
                        break;
                    }
                }
            }
            g_list_free(children);
        }
    }

    // 2. Zwolnij dane mapy
    if (cleanup_data->map_data_to_free) {
        g_print("on_map_window_delete_event_manual_cleanup: Wywoływanie free_index_map_data dla %p.\n", (void*)cleanup_data->map_data_to_free);
        free_index_map_data(cleanup_data->map_data_to_free);
        cleanup_data->map_data_to_free = NULL;
    } else {
        g_print("on_map_window_delete_event_manual_cleanup: map_data_to_free był już NULL.\n");
    }

    // 3. Reaktywuj aplikację (aby pokazać okno konfiguracji)
    if (cleanup_data->app) {
        g_print("on_map_window_delete_event_manual_cleanup: Ponowna aktywacja aplikacji.\n");
        g_application_activate(G_APPLICATION(cleanup_data->app));
    }

    // 4. Odłącz ten handler od sygnału
    g_print("on_map_window_delete_event_manual_cleanup: Odłączanie handlera od okna %p.\n", (void*)widget);
    g_signal_handlers_disconnect_by_func(widget,
                                         on_map_window_delete_event_manual_cleanup,
                                         cleanup_data);

    // 5. ODROCZONE zniszczenie okna mapy
    g_print("on_map_window_delete_event_manual_cleanup: Kolejkowanie zniszczenia okna mapy %p przez g_idle_add.\n", (void*)widget);
    g_object_ref(widget); // Dodaj referencję, bo widget będzie użyty w idle callback
    g_idle_add(destroy_widget_idle, widget);


    // 6. Zwolnij strukturę MapWindowCloseAndCleanupData
    g_print("on_map_window_delete_event_manual_cleanup: Zwalnianie struktury cleanup_data %p.\n", (void*)cleanup_data);
    g_free(cleanup_data);

    g_print("== on_map_window_delete_event_manual_cleanup: Zakończono (zniszczenie okna odroczone). ==\n");
    return TRUE; // Zdarzenie w pełni obsłużone
}


static void create_and_show_map_window(GtkApplication *app, gpointer user_data) {
    IndexMapData *passed_map_data = (IndexMapData*)user_data;

    if (passed_map_data == NULL) {
        fprintf(stderr, "create_and_show_map_window: Otrzymano NULL jako user_data.\n");
        return;
    }
    if (!passed_map_data->ndvi_data && !passed_map_data->ndmi_data) {
        fprintf(stderr, "create_and_show_map_window: Krytyczny błąd - dane NDVI lub NDMI w map_data są NULL.\n");
        free_index_map_data(passed_map_data);
        return;
    }

    GtkWidget *map_window;
    GtkWidget *map_main_vbox;
    GtkWidget *radio_map_hbox;
    GtkWidget *radio_ndmi, *radio_ndvi;
    GtkWidget *scrolled_window_for_map;
    GtkWidget *drawing_area_map;

    map_window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(map_window), "Wynikowa Mapa Wskaźników");
    gtk_window_set_default_size(GTK_WINDOW(map_window), DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT);
    gtk_container_set_border_width(GTK_CONTAINER(map_window), 10);

    MapWindowCloseAndCleanupData *cleanup_data_for_event = g_new(MapWindowCloseAndCleanupData, 1);
    cleanup_data_for_event->app = app;
    cleanup_data_for_event->map_data_to_free = passed_map_data;

    g_print("create_and_show_map_window: Podłączanie on_map_window_delete_event_manual_cleanup do 'delete-event'.\n");
    g_print("  map_data_to_free: %p\n", (void*)passed_map_data);
    g_signal_connect(map_window, "delete-event", G_CALLBACK(on_map_window_delete_event_manual_cleanup), cleanup_data_for_event);

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

    if (strcmp(passed_map_data->current_map_type, "NDVI") == 0) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_ndvi), TRUE);
    } else {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_ndmi), TRUE);
    }

    scrolled_window_for_map = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window_for_map), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand(scrolled_window_for_map, TRUE);
    gtk_widget_set_vexpand(scrolled_window_for_map, TRUE);
    gtk_box_pack_start(GTK_BOX(map_main_vbox), scrolled_window_for_map, TRUE, TRUE, 0);

    gtk_widget_set_size_request(drawing_area_map, passed_map_data->width, passed_map_data->height);
    g_signal_connect(drawing_area_map, "draw", G_CALLBACK(on_draw_map_area), passed_map_data);
    gtk_container_add(GTK_CONTAINER(scrolled_window_for_map), drawing_area_map);

    // g_print("Wyświetlanie okna mapy z danymi: NDVI (%p), NDMI (%p), Wymiary: %dx%d, Typ: %s\n",
    //         (void*)passed_map_data->ndvi_data, (void*)passed_map_data->ndmi_data,
    //         passed_map_data->width, passed_map_data->height, passed_map_data->current_map_type);

    gtk_widget_show_all(map_window);
}

// Handler dla zniszczenia okna konfiguracji
static void on_config_window_destroy(GtkWidget *widget, gpointer data) {
    g_print("on_config_window_destroy: Okno konfiguracji %p zniszczone. Ustawiam the_config_window na NULL.\n", (void*)widget);
    if (the_config_window == widget) {
        the_config_window = NULL;
    }
}

// ZMODYFIKOWANA activate_config_window z użyciem statycznego wskaźnika
static void activate_config_window(GtkApplication *app, gpointer user_data) {
    if (the_config_window) {
        if (gtk_widget_get_visible(the_config_window)) {
             g_print("activate_config_window: Okno konfiguracji już istnieje i jest widoczne, prezentuję %p.\n", (void*)the_config_window);
            gtk_window_present(GTK_WINDOW(the_config_window));
        } else {
            // Jeśli okno istnieje, ale nie jest widoczne (np. zostało ukryte), pokaż je
            g_print("activate_config_window: Okno konfiguracji już istnieje, ale nie jest widoczne, pokazuję %p.\n", (void*)the_config_window);
            gtk_widget_show_all(the_config_window);
            gtk_window_present(GTK_WINDOW(the_config_window));
        }
        return;
    }

    g_print("activate_config_window: Tworzenie nowego okna konfiguracji.\n");
    GtkWidget *config_window_local; // Użyj lokalnej zmiennej
    GtkWidget *main_vbox;
    GtkWidget *radio_hbox_config;
    GtkWidget *load_buttons_hbox;
    GtkWidget *radio_10m, *radio_20m;
    GtkWidget *btn_rozpocznij;

    config_window_local = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(config_window_local), "Konfiguracja Analizy");
    gtk_window_set_default_size(GTK_WINDOW(config_window_local), DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT);
    gtk_container_set_border_width(GTK_CONTAINER(config_window_local), 10);

    the_config_window = config_window_local; // Zapisz do statycznego wskaźnika
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

    btn_load_b04_widget = gtk_button_new_with_label("Wczytaj pasmo B04");
    GList *children_b04 = gtk_container_get_children(GTK_CONTAINER(btn_load_b04_widget)); if (children_b04 != NULL && GTK_IS_LABEL(children_b04->data)) { gtk_label_set_ellipsize(GTK_LABEL(children_b04->data), PANGO_ELLIPSIZE_END); } g_list_free(children_b04);
    g_signal_connect(btn_load_b04_widget, "clicked", G_CALLBACK(on_load_b04_clicked), config_window_local);
    gtk_box_pack_start(GTK_BOX(load_buttons_hbox), btn_load_b04_widget, TRUE, TRUE, 0);

    btn_load_b08_widget = gtk_button_new_with_label("Wczytaj pasmo B08");
    GList *children_b08 = gtk_container_get_children(GTK_CONTAINER(btn_load_b08_widget)); if (children_b08 != NULL && GTK_IS_LABEL(children_b08->data)) { gtk_label_set_ellipsize(GTK_LABEL(children_b08->data), PANGO_ELLIPSIZE_END); } g_list_free(children_b08);
    g_signal_connect(btn_load_b08_widget, "clicked", G_CALLBACK(on_load_b08_clicked), config_window_local);
    gtk_box_pack_start(GTK_BOX(load_buttons_hbox), btn_load_b08_widget, TRUE, TRUE, 0);

    btn_load_b11_widget = gtk_button_new_with_label("Wczytaj pasmo B11");
    GList *children_b11 = gtk_container_get_children(GTK_CONTAINER(btn_load_b11_widget)); if (children_b11 != NULL && GTK_IS_LABEL(children_b11->data)) { gtk_label_set_ellipsize(GTK_LABEL(children_b11->data), PANGO_ELLIPSIZE_END); } g_list_free(children_b11);
    g_signal_connect(btn_load_b11_widget, "clicked", G_CALLBACK(on_load_b11_clicked), config_window_local);
    gtk_box_pack_start(GTK_BOX(load_buttons_hbox), btn_load_b11_widget, TRUE, TRUE, 0);

    btn_load_scl_widget = gtk_button_new_with_label("Wczytaj pasmo SCL");
    GList *children_scl = gtk_container_get_children(GTK_CONTAINER(btn_load_scl_widget)); if (children_scl != NULL && GTK_IS_LABEL(children_scl->data)) { gtk_label_set_ellipsize(GTK_LABEL(children_scl->data), PANGO_ELLIPSIZE_END); } g_list_free(children_scl);
    g_signal_connect(btn_load_scl_widget, "clicked", G_CALLBACK(on_load_scl_clicked), config_window_local);
    gtk_box_pack_start(GTK_BOX(load_buttons_hbox), btn_load_scl_widget, TRUE, TRUE, 0);

    btn_rozpocznij = gtk_button_new_with_label("Rozpocznij");
    g_signal_connect(btn_rozpocznij, "clicked", G_CALLBACK(on_rozpocznij_clicked), config_window_local);
    gtk_box_pack_start(GTK_BOX(main_vbox), btn_rozpocznij, FALSE, FALSE, 0);
    gtk_widget_set_halign(btn_rozpocznij, GTK_ALIGN_START);

    gtk_widget_show_all(config_window_local);
}

static gboolean on_draw_map_area(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    IndexMapData *map_data = (IndexMapData*)g_object_get_data(G_OBJECT(widget), "map_data_ptr");

    if (!map_data) {
        fprintf(stderr, "on_draw_map_area: map_data_ptr na drawing_area jest NULL. Rysowanie tła.\n");
        cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
        cairo_paint(cr);
        return TRUE;
    }

    const float* data_to_render = NULL;
    if (strcmp(map_data->current_map_type, "NDVI") == 0) {
        data_to_render = map_data->ndvi_data;
    } else if (strcmp(map_data->current_map_type, "NDMI") == 0) {
        data_to_render = map_data->ndmi_data;
    }

    if (!data_to_render) {
        fprintf(stderr, "on_draw_map_area: data_to_render jest NULL dla typu mapy %s.\n", map_data->current_map_type);
        cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
        cairo_paint(cr);
        return TRUE;
    }

    GdkPixbuf *pixbuf = generate_pixbuf_from_index_data(data_to_render,
                                                        map_data->width,
                                                        map_data->height,
                                                        map_data->current_map_type);
    if (pixbuf) {
        gdk_cairo_set_source_pixbuf(cr, pixbuf, 0, 0);
        cairo_paint(cr);
        g_object_unref(pixbuf);
    } else {
        fprintf(stderr, "on_draw_map_area: Nie udało się wygenerować pixbuf.\n");
        cairo_set_source_rgb(cr, 1, 0, 0);
        cairo_paint(cr);
    }
    return TRUE;
}

int run_gui(int argc, char *argv[]) {
    GtkApplication *app;
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