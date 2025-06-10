#include "gui_utils.h"
#include <pango/pango.h>
#include <stdio.h>

GtkWidget* create_button_with_ellipsis(const char* label)
{
    GtkWidget* button = gtk_button_new_with_label(label);
    set_button_label_ellipsis(GTK_BUTTON(button));
    return button;
}

GtkWidget* create_file_dialog(GtkWindow* parent, const char* title)
{
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

void set_button_label_ellipsis(GtkButton* button)
{
    GList* children = gtk_container_get_children(GTK_CONTAINER(button));
    if (children != NULL)
    {
        GtkWidget* label_widget = GTK_WIDGET(children->data);
        if (GTK_IS_LABEL(label_widget))
        {
            gtk_label_set_ellipsize(GTK_LABEL(label_widget), PANGO_ELLIPSIZE_END);
        }
        g_list_free(children);
    }
}

void update_button_label(GtkButton* button, const char* filepath, const char* prefix)
{
    const char* short_name = get_short_filename(filepath);

    char new_label[256];
    snprintf(new_label, sizeof(new_label), "%s%s", prefix, short_name);
    gtk_button_set_label(button, new_label);

    // Ustawienie ellipsis dla długich nazw
    set_button_label_ellipsis(button);
}

gboolean destroy_widget_idle(gpointer data)
{
    GtkWidget* widget_to_destroy = GTK_WIDGET(data);
    // Sprawdź, czy widget nadal istnieje i jest widgetem GTK
    // gtk_widget_in_destruction sprawdza, czy widget jest już w trakcie niszczenia
    if (widget_to_destroy && GTK_IS_WIDGET(widget_to_destroy) && !gtk_widget_in_destruction(widget_to_destroy))
    {
        g_print("destroy_widget_idle: Niszczenie widgetu %p (odroczone).\n", (void*)widget_to_destroy);
        gtk_widget_destroy(widget_to_destroy);
    }
    else
    {
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
