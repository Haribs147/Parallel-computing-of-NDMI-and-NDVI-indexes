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

#define DEFAULT_WINDOW_WIDTH 900
#define DEFAULT_WINDOW_HEIGHT 750

// Deklaracje wprzód dla funkcji GUI
static void create_and_show_map_window(GtkApplication *app, gpointer user_data);
static void activate_config_window(GtkApplication *app, gpointer user_data);

// Zmienne globalne do przechowywania ścieżek
static char *path_b04 = NULL;
static char *path_b08 = NULL;
static char *path_b11 = NULL;
static char *path_scl = NULL;
static gboolean res_10m_selected = TRUE;

// Wskaźniki do widgetów przycisków
static GtkWidget *btn_load_b04_widget = NULL;
static GtkWidget *btn_load_b08_widget = NULL;
static GtkWidget *btn_load_b11_widget = NULL;
static GtkWidget *btn_load_scl_widget = NULL;

// Funkcja pomocnicza do obsługi wyboru pliku
static void handle_file_selection(GtkWindow *parent_window, const gchar *dialog_title_suffix, char **target_path_variable, GtkButton *button_to_update) {
    GtkWidget *dialog;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
    gint res;
    char dialog_full_title[100];
    char button_label_prefix[50];
    char default_button_text[100];

    // Ustalenie prefiksu dla etykiety przycisku oraz domyślnego tekstu przycisku
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

    dialog = gtk_file_chooser_dialog_new(dialog_full_title,
                                         parent_window,
                                         action,
                                         "_Anuluj", GTK_RESPONSE_CANCEL,
                                         "_Otwórz", GTK_RESPONSE_ACCEPT,
                                         NULL);

    GtkFileFilter *filter_jp2 = gtk_file_filter_new();
    gtk_file_filter_set_name(filter_jp2, "Obrazy JP2 (*.jp2)");
    gtk_file_filter_add_pattern(filter_jp2, "*.jp2");
    gtk_file_filter_add_pattern(filter_jp2, "*.JP2");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter_jp2);

    res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        char *filename_path;
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
        filename_path = gtk_file_chooser_get_filename(chooser);

        if (*target_path_variable) {
            g_free(*target_path_variable);
        }
        *target_path_variable = g_strdup(filename_path);

        g_print("Wybrano plik dla %s: %s\n", dialog_title_suffix, filename_path);

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

static void on_load_b04_clicked(GtkWidget *widget, gpointer data) {
    handle_file_selection(GTK_WINDOW(data), "pasma B04", &path_b04, GTK_BUTTON(widget));
}

static void on_load_b08_clicked(GtkWidget *widget, gpointer data) {
    handle_file_selection(GTK_WINDOW(data), "pasma B08", &path_b08, GTK_BUTTON(widget));
}

static void on_load_b11_clicked(GtkWidget *widget, gpointer data) {
    handle_file_selection(GTK_WINDOW(data), "pasma B11", &path_b11, GTK_BUTTON(widget));
}

static void on_load_scl_clicked(GtkWidget *widget, gpointer data) {
    handle_file_selection(GTK_WINDOW(data), "pasma SCL", &path_scl, GTK_BUTTON(widget));
}

static void on_rozpocznij_clicked(GtkWidget *widget, gpointer user_data) {
    GtkWidget *config_window_widget = GTK_WIDGET(user_data);
    GtkWindow *parent_gtk_window = GTK_WINDOW(user_data);

    g_print("Przycisk 'Rozpocznij' kliknięty.\n");

    if (!path_b04 || !path_b08 || !path_b11 || !path_scl) {
        g_print("BŁĄD: Nie wszystkie wymagane pasma zostały wybrane. Proszę wybrać wszystkie cztery pasma.\n");
        GtkWidget *error_dialog = gtk_message_dialog_new(parent_gtk_window,
                                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                                         GTK_MESSAGE_ERROR,
                                                         GTK_BUTTONS_CLOSE,
                                                         "Błąd: Nie wszystkie pasma zostały wybrane!\n\nProszę wybrać pliki dla pasm B04, B08, B11 oraz SCL.");
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

    pafBand4Data = LoadBandData(path_b04, &nBand4Width, &nBand4Height);
    if(pafBand4Data) printf("Wczytano B04 (%dx%d)\n", nBand4Width, nBand4Height); else fprintf(stderr, "Błąd wczytywania B04!\n");

    pafBand8Data = LoadBandData(path_b08, &nBand8Width, &nBand8Height);
    if(pafBand8Data) printf("Wczytano B08 (%dx%d)\n", nBand8Width, nBand8Height); else fprintf(stderr, "Błąd wczytywania B08!\n");

    pafBand11Data = LoadBandData(path_b11, &nBand11Width, &nBand11Height);
    if(pafBand11Data) printf("Wczytano B11 (%dx%d)\n", nBand11Width, nBand11Height); else fprintf(stderr, "Błąd wczytywania B11!\n");

    pafSCLData = LoadBandData(path_scl, &nSCLWidth, &nSCLHeight);
    if(pafSCLData) printf("Wczytano SCL (%dx%d)\n", nSCLWidth, nSCLHeight); else fprintf(stderr, "Błąd wczytywania SCL!\n");


    if (!pafBand4Data || !pafBand8Data || !pafBand11Data || !pafSCLData) {
        fprintf(stderr, "\nNie udało się wczytać jednego lub więcej pasm. Przetwarzanie przerwane.\n");
        GtkWidget *load_error_dialog = gtk_message_dialog_new(parent_gtk_window,
                                                              GTK_DIALOG_DESTROY_WITH_PARENT,
                                                              GTK_MESSAGE_ERROR,
                                                              GTK_BUTTONS_CLOSE,
                                                              "Błąd wczytywania pasm!\n\nNie udało się wczytać jednego lub więcej wymaganych plików pasm. Sprawdź komunikaty w konsoli.");
        gtk_window_set_title(GTK_WINDOW(load_error_dialog), "Błąd wczytywania danych");
        gtk_dialog_run(GTK_DIALOG(load_error_dialog));
        gtk_widget_destroy(load_error_dialog);

        if (pafBand4Data) free(pafBand4Data);
        if (pafBand8Data) free(pafBand8Data);
        if (pafBand11Data) free(pafBand11Data);
        if (pafSCLData) free(pafSCLData);
        return;
    }

    g_print("Wszystkie wymagane pasma wczytane pomyślnie.\n");
    
    // --- Przygotowanie wskaźników na finalne dane pasm po przetworzeniu ---
    float* pafBand4Processed = pafBand4Data;
    int nBand4ProcessedW = nBand4Width, nBand4ProcessedH = nBand4Height;
    float* pafBand8Processed = pafBand8Data;
    int nBand8ProcessedW = nBand8Width, nBand8ProcessedH = nBand8Height;
    float* pafBand11Processed = pafBand11Data;
    int nBand11ProcessedW = nBand11Width, nBand11ProcessedH = nBand11Height;
    float* pafSCLProcessed = pafSCLData;
    int nSCLProcessedW = nSCLWidth, nSCLProcessedH = nSCLHeight;


    int final_processing_width;
    int final_processing_height;

    if (res_10m_selected) {
        final_processing_width = nBand4Width; // Używamy wymiarów B04 jako referencji dla 10m
        final_processing_height = nBand4Height;
        g_print("Docelowa rozdzielczość: 10m (%dx%d)\n", final_processing_width, final_processing_height);

        // Upsample B11 (20m -> 10m)
        if (nBand11Width != final_processing_width || nBand11Height != final_processing_height) {
            g_print("Upsampling B11 do %dx%d...\n", final_processing_width, final_processing_height);
            float* resampled_b11 = bilinear_resample_float(pafBand11Data, nBand11Width, nBand11Height, final_processing_width, final_processing_height);
            if (resampled_b11) {
                if (pafBand11Processed != pafBand11Data) free(pafBand11Processed); // Jeśli był wcześniej jakiś "processed"
                free(pafBand11Data); // Zwolnij oryginalne 20m
                pafBand11Processed = resampled_b11;
                nBand11ProcessedW = final_processing_width;
                nBand11ProcessedH = final_processing_height;
                g_print("Upsampling B11 zakończony.\n");
            } else {
                //fprintf(stderr, "Błąd upsamplingu B11. Przetwarzanie przerwane.\n"); goto cleanup_and_exit;
            }
        }
        // Upsample SCL (20m -> 10m)
        if (nSCLWidth != final_processing_width || nSCLHeight != final_processing_height) {
             g_print("Upsampling SCL do %dx%d...\n", final_processing_width, final_processing_height);
            float* resampled_scl = nearest_neighbor_resample_scl(pafSCLData, nSCLWidth, nSCLHeight, final_processing_width, final_processing_height);
            if (resampled_scl) {
                if (pafSCLProcessed != pafSCLData) free(pafSCLProcessed);
                free(pafSCLData); // Zwolnij oryginalne 20m
                pafSCLProcessed = resampled_scl;
                nSCLProcessedW = final_processing_width;
                nSCLProcessedH = final_processing_height;
                g_print("Upsampling SCL zakończony.\n");
            } else {
                //fprintf(stderr, "Błąd upsamplingu SCL. Przetwarzanie przerwane.\n"); goto cleanup_and_exit;
            }
        }
        // B4 i B8 są już 10m
    } else { // res_20m_selected (downscaling do 20m)
        final_processing_width = nBand11Width; // Używamy wymiarów B11 jako referencji dla 20m
        final_processing_height = nBand11Height;
        g_print("Docelowa rozdzielczość: 20m (%dx%d)\n", final_processing_width, final_processing_height);

        // Downscale B04 (10m -> 20m)
        if (nBand4Width != final_processing_width || nBand4Height != final_processing_height) {
            g_print("Downsampling B04 do %dx%d...\n", final_processing_width, final_processing_height);
            float* resampled_b04 = average_resample_float(pafBand4Data, nBand4Width, nBand4Height, final_processing_width, final_processing_height);
            if (resampled_b04) {
                if (pafBand4Processed != pafBand4Data) free(pafBand4Processed);
                free(pafBand4Data); // Zwolnij oryginalne 10m
                pafBand4Processed = resampled_b04;
                nBand4ProcessedW = final_processing_width;
                nBand4ProcessedH = final_processing_height;
                 g_print("Downsampling B04 zakończony.\n");
            } else {
                //fprintf(stderr, "Błąd downsamplingu B04. Przetwarzanie przerwane.\n"); goto cleanup_and_exit;
            }
        }
        // Downscale B08 (10m -> 20m)
         if (nBand8Width != final_processing_width || nBand8Height != final_processing_height) {
            g_print("Downsampling B08 do %dx%d...\n", final_processing_width, final_processing_height);
            float* resampled_b08 = average_resample_float(pafBand8Data, nBand8Width, nBand8Height, final_processing_width, final_processing_height);
            if (resampled_b08) {
                if (pafBand8Processed != pafBand8Data) free(pafBand8Processed);
                free(pafBand8Data); // Zwolnij oryginalne 10m
                pafBand8Processed = resampled_b08;
                nBand8ProcessedW = final_processing_width;
                nBand8ProcessedH = final_processing_height;
                g_print("Downsampling B08 zakończony.\n");
            } else {
                //fprintf(stderr, "Błąd downsamplingu B08. Przetwarzanie przerwane.\n"); goto cleanup_and_exit;
            }
        }
        // B11 i SCL są już 20m
    }

    GtkApplication *app = gtk_window_get_application(GTK_WINDOW(config_window_widget));
    create_and_show_map_window(app, NULL );
    gtk_widget_destroy(config_window_widget);
    
    // Pamiętaj o zwolnieniu pamięci dla danych pasm, gdy nie będą już potrzebne
    // (np. w oknie mapy lub po jego zamknięciu)
    // if (pafBand4Data) free(pafBand4Data); // Przykład
    // if (pafBand8Data) free(pafBand8Data); // Przykład
    // if (pafBand11Data) free(pafBand11Data); // Przykład
    // if (pafSCLProcessedData && pafSCLProcessedData != pafSCLData) free(pafSCLProcessedData); // Przykład, jeśli SCL było resamplowane
    // else if (pafSCLProcessedData == pafSCLData && pafSCLData != NULL) { /* pafSCLData zostanie zwolnione później lub nie było resamplowane */ }


    g_print("Dane pasm (i SCL) przekazane (koncepcyjnie) do okna mapy. Zarządzanie pamięcią TODO.\n");
}


static void on_config_radio_resolution_toggled(GtkToggleButton *togglebutton, gpointer data) {
    if (gtk_toggle_button_get_active(togglebutton)) {
        const gchar *label = gtk_button_get_label(GTK_BUTTON(togglebutton));
        if (g_str_has_prefix(label, "upscaling")) {
            res_10m_selected = TRUE;
        } else if (g_str_has_prefix(label, "downscaling")) {
            res_10m_selected = FALSE;
        }
        g_print("Wybrano rozdzielczość (konfiguracja): %s\n", res_10m_selected ? "10m" : "20m");
    }
}

static void on_map_type_radio_toggled(GtkToggleButton *togglebutton, gpointer data) {
    if (gtk_toggle_button_get_active(togglebutton)) {
        const gchar *label = gtk_button_get_label(GTK_BUTTON(togglebutton));
        g_print("Wybrano typ mapy do wyświetlenia: %s\n", label);
    }
}

static void create_and_show_map_window(GtkApplication *app, gpointer user_data) {
    GtkWidget *map_window;
    GtkWidget *map_main_vbox;
    GtkWidget *radio_map_hbox;
    GtkWidget *radio_ndmi, *radio_ndvi;
    GtkWidget *frame_for_map;
    GtkWidget *drawing_area_map;

    map_window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(map_window), "Wynikowa Mapa");
    gtk_window_set_default_size(GTK_WINDOW(map_window), DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT);
    gtk_container_set_border_width(GTK_CONTAINER(map_window), 10);

    map_main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(map_window), map_main_vbox);

    radio_map_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(radio_map_hbox, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(map_main_vbox), radio_map_hbox, FALSE, FALSE, 0);

    radio_ndmi = gtk_radio_button_new_with_label(NULL, "NDMI");
    g_signal_connect(radio_ndmi, "toggled", G_CALLBACK(on_map_type_radio_toggled), NULL);
    gtk_box_pack_start(GTK_BOX(radio_map_hbox), radio_ndmi, FALSE, FALSE, 0);

    radio_ndvi = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio_ndmi), "NDVI");
    g_signal_connect(radio_ndvi, "toggled", G_CALLBACK(on_map_type_radio_toggled), NULL);
    gtk_box_pack_start(GTK_BOX(radio_map_hbox), radio_ndvi, FALSE, FALSE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_ndvi), TRUE);

    frame_for_map = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame_for_map), GTK_SHADOW_ETCHED_IN);
    gtk_box_pack_start(GTK_BOX(map_main_vbox), frame_for_map, TRUE, TRUE, 0);

    drawing_area_map = gtk_drawing_area_new();
    gtk_container_add(GTK_CONTAINER(frame_for_map), drawing_area_map);

    gtk_widget_show_all(map_window);
}

static void activate_config_window(GtkApplication *app, gpointer user_data) {
    GtkWidget *config_window;
    GtkWidget *main_vbox;
    GtkWidget *radio_hbox_config;
    GtkWidget *load_buttons_hbox;
    GtkWidget *radio_10m, *radio_20m;
    GtkWidget *btn_rozpocznij;

    config_window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(config_window), "Konfiguracja Analizy");
    gtk_window_set_default_size(GTK_WINDOW(config_window), DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT);
    gtk_container_set_border_width(GTK_CONTAINER(config_window), 10);

    main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(config_window), main_vbox);

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
    GList *children_b04 = gtk_container_get_children(GTK_CONTAINER(btn_load_b04_widget));
    if (children_b04 != NULL && GTK_IS_LABEL(children_b04->data)) {
        gtk_label_set_ellipsize(GTK_LABEL(children_b04->data), PANGO_ELLIPSIZE_END);
    }
    g_list_free(children_b04);
    g_signal_connect(btn_load_b04_widget, "clicked", G_CALLBACK(on_load_b04_clicked), config_window);
    gtk_box_pack_start(GTK_BOX(load_buttons_hbox), btn_load_b04_widget, TRUE, TRUE, 0);

    btn_load_b08_widget = gtk_button_new_with_label("Wczytaj pasmo B08");
    GList *children_b08 = gtk_container_get_children(GTK_CONTAINER(btn_load_b08_widget));
    if (children_b08 != NULL && GTK_IS_LABEL(children_b08->data)) {
        gtk_label_set_ellipsize(GTK_LABEL(children_b08->data), PANGO_ELLIPSIZE_END);
    }
    g_list_free(children_b08);
    g_signal_connect(btn_load_b08_widget, "clicked", G_CALLBACK(on_load_b08_clicked), config_window);
    gtk_box_pack_start(GTK_BOX(load_buttons_hbox), btn_load_b08_widget, TRUE, TRUE, 0);

    btn_load_b11_widget = gtk_button_new_with_label("Wczytaj pasmo B11");
    GList *children_b11 = gtk_container_get_children(GTK_CONTAINER(btn_load_b11_widget));
    if (children_b11 != NULL && GTK_IS_LABEL(children_b11->data)) {
        gtk_label_set_ellipsize(GTK_LABEL(children_b11->data), PANGO_ELLIPSIZE_END);
    }
    g_list_free(children_b11);
    g_signal_connect(btn_load_b11_widget, "clicked", G_CALLBACK(on_load_b11_clicked), config_window);
    gtk_box_pack_start(GTK_BOX(load_buttons_hbox), btn_load_b11_widget, TRUE, TRUE, 0);

    btn_load_scl_widget = gtk_button_new_with_label("Wczytaj pasmo SCL");
    GList *children_scl = gtk_container_get_children(GTK_CONTAINER(btn_load_scl_widget));
    if (children_scl != NULL && GTK_IS_LABEL(children_scl->data)) {
        gtk_label_set_ellipsize(GTK_LABEL(children_scl->data), PANGO_ELLIPSIZE_END);
    }
    g_list_free(children_scl);
    g_signal_connect(btn_load_scl_widget, "clicked", G_CALLBACK(on_load_scl_clicked), config_window);
    gtk_box_pack_start(GTK_BOX(load_buttons_hbox), btn_load_scl_widget, TRUE, TRUE, 0);

    btn_rozpocznij = gtk_button_new_with_label("Rozpocznij");
    g_signal_connect(btn_rozpocznij, "clicked", G_CALLBACK(on_rozpocznij_clicked), config_window);
    gtk_box_pack_start(GTK_BOX(main_vbox), btn_rozpocznij, FALSE, FALSE, 0);
    gtk_widget_set_halign(btn_rozpocznij, GTK_ALIGN_START);

    gtk_widget_show_all(config_window);
}

int run_gui(int argc, char *argv[]) {
    GtkApplication *app;
    int status;

    GDALAllRegister();

    app = gtk_application_new("org.projekt.sentinelgui", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate_config_window), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    if (path_b04) g_free(path_b04);
    if (path_b08) g_free(path_b08);
    if (path_b11) g_free(path_b11);
    if (path_scl) g_free(path_scl);

    return status;
}
