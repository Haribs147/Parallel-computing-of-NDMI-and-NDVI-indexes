#include "gui_utils.h"
#include <pango/pango.h>
#include <stdio.h>

const BandConfig band_configs[] = {
    {"pasma B04", "B04: ", "Wczytaj pasmo B04"},
    {"pasma B08", "B08: ", "Wczytaj pasmo B08"},
    {"pasma B11", "B11: ", "Wczytaj pasmo B11"},
    {"pasma SCL", "SCL: ", "Wczytaj pasmo SCL"},
    {NULL, "Plik: ", "Wybierz plik"} // default
};

GtkWidget* create_button_with_ellipsis(const char* label) {
    GtkWidget* button = gtk_button_new_with_label(label);
    set_button_label_ellipsis(GTK_BUTTON(button));
    return button;
}

GtkWidget* create_file_dialog(GtkWindow* parent, const char* title) {
    GtkWidget* dialog = gtk_file_chooser_dialog_new(
        title, parent, GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Anuluj", GTK_RESPONSE_CANCEL,
        "_Otwórz", GTK_RESPONSE_ACCEPT, NULL);

    // Filtr JP2
    GtkFileFilter* filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Obrazy JP2 (*.jp2)");
    gtk_file_filter_add_pattern(filter, "*.jp2");
    gtk_file_filter_add_pattern(filter, "*.JP2");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    return dialog;
}

void set_button_label_ellipsis(GtkButton* button) {
    GList* children = gtk_container_get_children(GTK_CONTAINER(button));
    if (children != NULL) {
        GtkWidget* label_widget = GTK_WIDGET(children->data);
        if (GTK_IS_LABEL(label_widget)) {
            gtk_label_set_ellipsize(GTK_LABEL(label_widget), PANGO_ELLIPSIZE_END);
        }
        g_list_free(children);
    }
}

void update_button_label(GtkButton* button, const char* filepath, const char* prefix) {
    const char* short_name = get_short_filename(filepath);

    char new_label[256];
    snprintf(new_label, sizeof(new_label), "%s%s", prefix, short_name);
    gtk_button_set_label(button, new_label);

    // Ustawienie ellipsis dla długich nazw
    set_button_label_ellipsis(button);
}

const BandConfig* get_band_config(const gchar* dialog_title_suffix) {
    for (int i = 0; band_configs[i].suffix != NULL; i++) {
        if (g_strcmp0(dialog_title_suffix, band_configs[i].suffix) == 0) {
            return &band_configs[i];
        }
    }
    return &band_configs[4]; // default
}

void update_file_selection(char** target_path_variable, GtkButton* button_to_update,
                          GtkFileChooser* file_chooser, const BandConfig* config) {
    char* filename_path = gtk_file_chooser_get_filename(file_chooser);
    if (!filename_path) {
        return; // Brak wybranego pliku
    }

    // Zwolnienie starej ścieżki
    if (*target_path_variable) {
        g_free(*target_path_variable);
    }
    *target_path_variable = g_strdup(filename_path);

    // Aktualizacja etykiety przycisku
    update_button_label(button_to_update, filename_path, config->prefix);

    g_free(filename_path);
}

void handle_file_selection(GtkWindow* parent_window, const gchar* dialog_title_suffix,
                          char** target_path_variable, GtkButton* button_to_update) {
    const BandConfig* config = get_band_config(dialog_title_suffix);

    char dialog_title[150];
    snprintf(dialog_title, sizeof(dialog_title), "Wybierz plik dla %s", dialog_title_suffix);

    GtkWidget* dialog = create_file_dialog(parent_window, dialog_title);
    gint res = gtk_dialog_run(GTK_DIALOG(dialog));

    if (res == GTK_RESPONSE_ACCEPT) {
        update_file_selection(target_path_variable, button_to_update,
                             GTK_FILE_CHOOSER(dialog), config);
    } else if (!(*target_path_variable)) {
        gtk_button_set_label(button_to_update, config->default_text);
    }

    gtk_widget_destroy(dialog);
}

gboolean destroy_widget_idle(gpointer data) {
    GtkWidget* widget_to_destroy = GTK_WIDGET(data);
    // Sprawdź, czy widget nadal istnieje i jest widgetem GTK
    // gtk_widget_in_destruction sprawdza, czy widget jest już w trakcie niszczenia
    if (widget_to_destroy && GTK_IS_WIDGET(widget_to_destroy) && !gtk_widget_in_destruction(widget_to_destroy)) {
        g_print("destroy_widget_idle: Niszczenie widgetu %p (odroczone).\n", (void*)widget_to_destroy);
        gtk_widget_destroy(widget_to_destroy);
    } else {
        g_print(
            "destroy_widget_idle: Widget %p już zniszczony, w trakcie niszczenia lub nie jest widgetem. Nie niszczę ponownie.\n",
            (void*)widget_to_destroy);
    }
    g_object_unref(widget_to_destroy); // Zwolnij referencję dodaną dla g_idle_add
    return G_SOURCE_REMOVE; // Usuń źródło bezczynności po wykonaniu
}

void show_error_dialog(GtkWindow* parent_window, const char* message)
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
