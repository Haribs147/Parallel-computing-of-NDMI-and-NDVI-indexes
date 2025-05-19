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
    float* pafBand4ProcessedData = pafBand4Data;
    int nBand4ProcessedWidth = nBand4Width;
    int nBand4ProcessedHeight = nBand4Height;

    float* pafBand8ProcessedData = pafBand8Data;
    int nBand8ProcessedWidth = nBand8Width;
    int nBand8ProcessedHeight = nBand8Height;

    float* pafBand11ProcessedData = pafBand11Data;
    int nBand11ProcessedWidth = nBand11Width;
    int nBand11ProcessedHeight = nBand11Height;

    float* pafSCLProcessedData = pafSCLData;
    int nSCLProcessedWidth = nSCLWidth;
    int nSCLProcessedHeight = nSCLHeight;

    int target_10m_width = nBand4Width;
    int target_10m_height = nBand4Height;

    if (res_10m_selected) {
        // Sprawdzamy, czy B11 faktycznie wymaga upsamplingu do wymiarów 10m
        if (nBand11Width != target_10m_width || nBand11Height != target_10m_height) {
            g_print("Rozpoczynanie upscalingu B11 (SWIR1) do %dx%d (Interpolacja dwuliniowa)...\n", target_10m_width, target_10m_height);
            float* resampled_b11_data = bilinear_resample_float(
                pafBand11Data,          // Oryginalne dane B11 (20m)
                nBand11Width,           // Szerokość oryginalna B11
                nBand11Height,          // Wysokość oryginalna B11
                target_10m_width,       // Docelowa szerokość (10m, z B04)
                target_10m_height       // Docelowa wysokość (10m, z B04)
            );

            if (!resampled_b11_data) {
                fprintf(stderr, "Błąd podczas upscalingu B11. Użycie oryginalnych danych B11 (20m).\n");
                // pafBand11ProcessedData nadal wskazuje na oryginalne pafBand11Data
            } else {
                g_print("Upscaling B11 (SWIR1) zakończony.\n");
                // Jeśli pafBand11ProcessedData wskazywało na coś innego (np. z poprzedniego, nieudanego etapu), zwolnij to.
                // W tym konkretnym przepływie, pafBand11ProcessedData == pafBand11Data przed tą operacją.
                if (pafBand11ProcessedData != pafBand11Data) { // Dodatkowe zabezpieczenie
                     free(pafBand11ProcessedData);
                }
                free(pafBand11Data); // Zwolnij oryginalne dane B11 (20m), bo mamy nowe
                pafBand11ProcessedData = resampled_b11_data; // Użyj nowych, przeskalowanych danych
                nBand11ProcessedWidth = target_10m_width;
                nBand11ProcessedHeight = target_10m_height;
            }
        } else {
            g_print("Upscaling B11 nie jest wymagany (wymiary już pasują do 10m).\n");
            // pafBand11ProcessedData nadal wskazuje na pafBand11Data
        }
    } else {
        // Jeśli wybrano "downscaling do 20m", pasmo B11 jest już 20m, więc nie robimy nic.
        g_print("B11 (SWIR1) pozostaje w oryginalnej rozdzielczości 20m.\n");
        // pafBand11ProcessedData nadal wskazuje na pafBand11Data
    }


    // --- ZMODYFIKOWANO: Resampling SCL (Nearest Neighbor) ---
    int targetSclWidth = nSCLWidth;     // Domyślnie oryginalne wymiary SCL
    int targetSclHeight = nSCLHeight;

    if (res_10m_selected) { // Jeśli docelowa rozdzielczość to 10m, użyj wymiarów 10m
        targetSclWidth = target_10m_width;
        targetSclHeight = target_10m_height;
    } 
    // Jeśli wybrano "downscaling do 20m", SCL jest już 20m, więc targetSclWidth/Height pozostaną nSCLWidth/Height

    // Sprawdzamy, czy SCL faktycznie wymaga resamplingu
    if (nSCLWidth != targetSclWidth || nSCLHeight != targetSclHeight) {
        g_print("Rozpoczynanie resamplingiem SCL do %dx%d (Najbliższy Sąsiad)...\n", targetSclWidth, targetSclHeight);
        float* resampled_scl_data = nearest_neighbor_resample_scl(
            pafSCLData,             // Oryginalne dane SCL
            nSCLWidth,              // Szerokość oryginalna SCL
            nSCLHeight,             // Wysokość oryginalna SCL
            targetSclWidth,         // Docelowa szerokość
            targetSclHeight         // Docelowa wysokość
        );
        if (!resampled_scl_data) {
            fprintf(stderr, "Błąd podczas resamplingiem SCL. Użycie oryginalnych danych SCL.\n");
            // pafSCLProcessedData nadal wskazuje na oryginalne pafSCLData
        } else {
            g_print("Resampling SCL zakończony.\n");
            // Podobnie jak dla B11, jeśli pafSCLProcessedData wskazywało na coś innego.
            if (pafSCLProcessedData != pafSCLData) {
                free(pafSCLProcessedData);
            }
            free(pafSCLData); // Zwolnij oryginalne dane SCL, bo mamy nowe
            pafSCLProcessedData = resampled_scl_data;
            nSCLProcessedWidth = targetSclWidth;
            nSCLProcessedHeight = targetSclHeight;
        }
    } else {
        g_print("Resampling SCL nie jest wymagany (wymiary już pasują do docelowych).\n");
        // pafSCLProcessedData nadal wskazuje na pafSCLData
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
