/*
 * Ten plik zawiera implementacje funkcjonalności GUI,
 * które są przydatne w module GUI. Nie ma tutaj logiki
 * biznesowej, jedynie funkcje typu "utwórz przycisk"
 * lub "zmień nagłówek"
*/
#ifndef GUI_UTILS_H
#define GUI_UTILS_H

#include <gtk/gtk.h>
#include "utils.h"

// Funkcje do tworzenia i konfigurowania widgetów
GtkWidget* create_button_with_ellipsis(const char* label);
GtkWidget* create_file_dialog(GtkWindow* parent, const char* title);

// Funkcje do obsługi przycisków i etykiet
void set_button_label_ellipsis(GtkButton* button);
void update_button_label(GtkButton* button, const char* filepath, const char* prefix);
void show_error_dialog(GtkWindow* parent_window, const char* message);

// Funkcja pomocnicza do odroczonego niszczenia widgetu
gboolean destroy_widget_idle(gpointer data);

#endif // GUI_UTILS_H
