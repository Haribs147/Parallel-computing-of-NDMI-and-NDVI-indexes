#include <gtk/gtk.h>
#include <gdal.h>   // Dla GDALAllRegister (jest w run_gui)
#include <stdio.h>  // Dla printf, fprintf
#include <stdlib.h> // Dla malloc, free, g_free
#include <math.h>   // Dla ceilf, sqrtf, floorf, roundf
#include <float.h>  // Dla FLT_MAX

#include "gui.h"         // Dołączamy własny plik nagłówkowy
#include "data_loader.h" // Dołączamy nagłówek dla LoadBandData

#define DEFAULT_WINDOW_WIDTH 700
#define DEFAULT_WINDOW_HEIGHT 650

// Deklaracje wprzód dla funkcji GUI
static void create_and_show_map_window(GtkApplication *app, gpointer user_data);
static void activate_config_window(GtkApplication *app, gpointer user_data);

// Zmienne globalne do przechowywania ścieżek (uproszczenie)
static char *path_b04 = NULL;
static char *path_b08 = NULL;
static char *path_b11 = NULL;
static char *path_scl = NULL;
static gboolean res_10m_selected = TRUE; // Domyślnie 10m

// Funkcja pomocnicza do obsługi wyboru pliku
static void handle_file_selection(GtkWindow *parent_window, const gchar *dialog_title_suffix, char **target_path_variable) {
    GtkWidget *dialog;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
    gint res;
    char dialog_full_title[100];

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

    GtkFileFilter *filter_all = gtk_file_filter_new();
    gtk_file_filter_set_name(filter_all, "Wszystkie pliki (*.*)");
    gtk_file_filter_add_pattern(filter_all, "*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter_all);

    res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        char *filename;
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
        filename = gtk_file_chooser_get_filename(chooser);

        if (*target_path_variable) {
            g_free(*target_path_variable); // g_free dla pamięci alokowanej przez g_strdup
        }
        *target_path_variable = g_strdup(filename); // Zapisz nową ścieżkę (kopia)

        g_print("Wybrano plik dla %s: %s\n", dialog_title_suffix, filename);
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

static void on_load_b04_clicked(GtkWidget *widget, gpointer data) {
    handle_file_selection(GTK_WINDOW(data), "pasma B04", &path_b04);
}

static void on_load_b08_clicked(GtkWidget *widget, gpointer data) {
    handle_file_selection(GTK_WINDOW(data), "pasma B08", &path_b08);
}

static void on_load_b11_clicked(GtkWidget *widget, gpointer data) {
    handle_file_selection(GTK_WINDOW(data), "pasma B11", &path_b11);
}

static void on_load_scl_clicked(GtkWidget *widget, gpointer data) {
    handle_file_selection(GTK_WINDOW(data), "pasma SCL", &path_scl);
}

static void on_rozpocznij_clicked(GtkWidget *widget, gpointer user_data) {
    GtkWidget *config_window_widget = GTK_WIDGET(user_data); // Renamed for clarity
    GtkWindow *config_window = GTK_WINDOW(user_data); // For gtk_message_dialog_new parent

    g_print("Przycisk 'Rozpocznij' kliknięty.\n");
    g_print("Sprawdzanie wybranych ścieżek:\n  B04: %s\n  B08: %s\n  B11: %s\n  SCL: %s\nRozdzielczość: %s\n",
            path_b04 ? path_b04 : "(nie wybrano)",
            path_b08 ? path_b08 : "(nie wybrano)",
            path_b11 ? path_b11 : "(nie wybrano)",
            path_scl ? path_scl : "(nie wybrano)",
            res_10m_selected ? "10m" : "20m");

    // Sprawdzenie, czy wszystkie ścieżki zostały wybrane
    if (!path_b04 || !path_b08 || !path_b11 || !path_scl) {
        g_print("BŁĄD: Nie wszystkie wymagane pasma zostały wybrane. Proszę wybrać wszystkie cztery pasma.\n");
        
        GtkWidget *error_dialog = gtk_message_dialog_new(config_window, // Użyj config_window jako rodzica
                                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                                         GTK_MESSAGE_ERROR,
                                                         GTK_BUTTONS_CLOSE,
                                                         "Błąd: Nie wszystkie pasma zostały wybrane!\n\nProszę wybrać pliki dla pasm B04, B08, B11 oraz SCL.");
        gtk_window_set_title(GTK_WINDOW(error_dialog), "Błąd wyboru plików");
        gtk_dialog_run(GTK_DIALOG(error_dialog));
        gtk_widget_destroy(error_dialog);
        return; // Nie kontynuuj
    }

    g_print("\nRozpoczynanie wczytywania danych pasm...\n");
    float* pafBand4Data = NULL; int nBand4Width = 0, nBand4Height = 0;
    float* pafBand8Data = NULL; int nBand8Width = 0, nBand8Height = 0;
    float* pafBand11Data = NULL; int nBand11Width = 0, nBand11Height = 0;
    float* pafSCLData = NULL; int nSCLWidth = 0, nSCLHeight = 0;
    gboolean all_bands_loaded_successfully = TRUE;

     // Próba wczytania wszystkich pasm
    pafBand4Data = LoadBandData(path_b04, &nBand4Width, &nBand4Height);
    pafBand8Data = LoadBandData(path_b08, &nBand8Width, &nBand8Height);
    pafBand11Data = LoadBandData(path_b11, &nBand11Width, &nBand11Height);
    pafSCLData = LoadBandData(path_scl, &nSCLWidth, &nSCLHeight);

    // Sprawdzenie, czy którekolwiek pasmo NIE zostało pomyślnie wczytane (odwrócona logika)
    if (!pafBand4Data || !pafBand8Data || !pafBand11Data || !pafSCLData) {
        fprintf(stderr, "\nNie udało się wczytać jednego lub więcej pasm. Przetwarzanie przerwane.\n");
        GtkWidget *load_error_dialog = gtk_message_dialog_new(config_window_widget,
                                                              GTK_DIALOG_DESTROY_WITH_PARENT,
                                                              GTK_MESSAGE_ERROR,
                                                              GTK_BUTTONS_CLOSE,
                                                              "Błąd wczytywania pasm!\n\nNie udało się wczytać jednego lub więcej wymaganych plików pasm.");
        gtk_window_set_title(GTK_WINDOW(load_error_dialog), "Błąd wczytywania danych");
        gtk_dialog_run(GTK_DIALOG(load_error_dialog));
        gtk_widget_destroy(load_error_dialog);

        // Zwolnienie pamięci dla pasm, które mogły zostać pomyślnie wczytane przed błędem
        if (pafBand4Data) free(pafBand4Data);
        if (pafBand8Data) free(pafBand8Data);
        if (pafBand11Data) free(pafBand11Data);
        if (pafSCLData) free(pafSCLData);
        // Okno konfiguracji pozostaje otwarte
        return;
    }
    
    // Jeśli doszliśmy tutaj, wszystkie pasma zostały wczytane pomyślnie
    g_print("Wszystkie wymagane pasma wczytane pomyślnie.\n");
    
    // TODO: Tutaj logika resamplingiem i przekazanie danych do create_and_show_map_window
    // Np. stworzyć strukturę AppData, wypełnić ją załadowanymi i przeskalowanymi danymi
    // i przekazać wskaźnik do niej jako user_data.
    // Pamiętaj o zarządzaniu pamięcią dla pafBandXData - jeśli są przekazywane,
    // okno mapy lub kolejna funkcja powinna być odpowiedzialna za ich zwolnienie.
    
    // Przykład: Przekazanie danych (na razie tylko wskaźniki, bez struktury)
    // Należałoby stworzyć strukturę, np.
    // typedef struct {
    //     float* b4_data; int b4_w, b4_h;
    //     // ... reszta pasm ...
    //     gboolean target_10m;
    // } AppProcessingData;
    // AppProcessingData* processing_data = g_new(AppProcessingData, 1);
    // processing_data->b4_data = pafBand4Data; // itd.
    // ... i przekazać processing_data do create_and_show_map_window
    // oraz zaimplementować funkcję zwalniającą tę strukturę i jej zawartość.

    GtkApplication *app = gtk_window_get_application(GTK_WINDOW(config_window_widget));
    create_and_show_map_window(app, NULL /* Tutaj wskaźnik do struktury z danymi */);
    gtk_widget_destroy(config_window_widget);

    // WAŻNE: Jeśli dane nie są przekazywane do create_and_show_map_window
    // (lub jeśli create_and_show_map_window kopiuje dane i nie przejmuje odpowiedzialności za zwolnienie),
    // to pamięć po pafBandXData powinna być zwolniona tutaj.
    // Na razie zakładamy, że okno mapy (lub dalsza logika) przejmie odpowiedzialność.
    // To jest kluczowy element zarządzania pamięcią do dopracowania.
    // Jeśli dane są przekazywane do okna mapy, to ono powinno je zwolnić przy swoim zniszczeniu.
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
        // TODO: Załaduj lub przerysuj odpowiednią mapę
    }
}

static void create_and_show_map_window(GtkApplication *app, gpointer user_data) {
    // user_data tutaj powinno zawierać wskaźnik do struktury z załadowanymi danymi pasm
    // i innymi potrzebnymi informacjami.
    // Np. AppData* data = (AppData*)user_data;

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
    g_signal_connect(radio_ndmi, "toggled", G_CALLBACK(on_map_type_radio_toggled), NULL /* drawing_area_map */);
    gtk_box_pack_start(GTK_BOX(radio_map_hbox), radio_ndmi, FALSE, FALSE, 0);

    radio_ndvi = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio_ndmi), "NDVI");
    g_signal_connect(radio_ndvi, "toggled", G_CALLBACK(on_map_type_radio_toggled), NULL /* drawing_area_map */);
    gtk_box_pack_start(GTK_BOX(radio_map_hbox), radio_ndvi, FALSE, FALSE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_ndvi), TRUE);

    frame_for_map = gtk_frame_new(NULL); 
    gtk_frame_set_shadow_type(GTK_FRAME(frame_for_map), GTK_SHADOW_ETCHED_IN); 
    gtk_box_pack_start(GTK_BOX(map_main_vbox), frame_for_map, TRUE, TRUE, 0);

    drawing_area_map = gtk_drawing_area_new();
    // TODO: Podłącz sygnał "draw" do drawing_area_map, aby rysować mapę
    // g_signal_connect(drawing_area_map, "draw", G_CALLBACK(on_draw_map_area), user_data);
    gtk_container_add(GTK_CONTAINER(frame_for_map), drawing_area_map);

    // WAŻNE: Jeśli user_data zawierało dynamicznie alokowane dane (np. pafBandXData),
    // a okno mapy jest odpowiedzialne za ich zwolnienie, podłącz sygnał "destroy" do map_window,
    // aby zwolnić te dane.
    // g_signal_connect(map_window, "destroy", G_CALLBACK(on_map_window_destroy_free_data), user_data);


    gtk_widget_show_all(map_window);
}

static void activate_config_window(GtkApplication *app, gpointer user_data) {
    GtkWidget *config_window; 
    GtkWidget *main_vbox; 
    GtkWidget *radio_hbox_config; 
    GtkWidget *load_buttons_hbox; 
    GtkWidget *radio_10m, *radio_20m;
    GtkWidget *btn_load_b04, *btn_load_b08, *btn_load_b11, *btn_load_scl;
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

    btn_load_b04 = gtk_button_new_with_label("Wczytaj pasmo B04");
    g_signal_connect(btn_load_b04, "clicked", G_CALLBACK(on_load_b04_clicked), config_window);
    gtk_box_pack_start(GTK_BOX(load_buttons_hbox), btn_load_b04, TRUE, TRUE, 0);

    btn_load_b08 = gtk_button_new_with_label("Wczytaj pasmo B08");
    g_signal_connect(btn_load_b08, "clicked", G_CALLBACK(on_load_b08_clicked), config_window);
    gtk_box_pack_start(GTK_BOX(load_buttons_hbox), btn_load_b08, TRUE, TRUE, 0);

    btn_load_b11 = gtk_button_new_with_label("Wczytaj pasmo B11");
    g_signal_connect(btn_load_b11, "clicked", G_CALLBACK(on_load_b11_clicked), config_window);
    gtk_box_pack_start(GTK_BOX(load_buttons_hbox), btn_load_b11, TRUE, TRUE, 0);

    btn_load_scl = gtk_button_new_with_label("Wczytaj pasmo SCL");
    g_signal_connect(btn_load_scl, "clicked", G_CALLBACK(on_load_scl_clicked), config_window);
    gtk_box_pack_start(GTK_BOX(load_buttons_hbox), btn_load_scl, TRUE, TRUE, 0);

    btn_rozpocznij = gtk_button_new_with_label("Rozpocznij");
    g_signal_connect(btn_rozpocznij, "clicked", G_CALLBACK(on_rozpocznij_clicked), config_window); 
    gtk_box_pack_start(GTK_BOX(main_vbox), btn_rozpocznij, FALSE, FALSE, 0);
    gtk_widget_set_halign(btn_rozpocznij, GTK_ALIGN_START);

    gtk_widget_show_all(config_window);
}

// Główna funkcja GUI, wywoływana z main.c
int run_gui(int argc, char *argv[]) {
    GtkApplication *app;
    int status;

    GDALAllRegister(); // Inicjalizacja GDAL raz na początku

    app = gtk_application_new("org.projekt.sentinelgui", G_APPLICATION_FLAGS_NONE); 
    g_signal_connect(app, "activate", G_CALLBACK(activate_config_window), NULL); 
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    // Zwolnienie globalnie przechowywanych ścieżek
    if (path_b04) g_free(path_b04);
    if (path_b08) g_free(path_b08);
    if (path_b11) g_free(path_b11);
    if (path_scl) g_free(path_scl);

    return status;
}
