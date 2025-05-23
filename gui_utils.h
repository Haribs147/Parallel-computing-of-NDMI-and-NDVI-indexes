#ifndef GUI_UTILS_H
#define GUI_UTILS_H

#include <gtk/gtk.h>
#include "utils.h"

typedef struct {
    const char* suffix;
    const char* prefix;
    const char* default_text;
} BandConfig;

typedef struct {
    char** path;
    GtkWidget** button;
    float** raw_data;
    float** processed_data;
    int* width;
    int* height;
    const char* band_name;
    int band_index;
} BandData;

enum BandType
{
    B04,
    B08,
    B11,
    SCL
};

extern const BandConfig band_configs[];

// Funkcje do tworzenia i konfigurowania widgetów
GtkWidget* create_button_with_ellipsis(const char* label);
GtkWidget* create_file_dialog(GtkWindow* parent, const char* title);

// Funkcje do obsługi przycisków i etykiet
void set_button_label_ellipsis(GtkButton* button);
void update_button_label(GtkButton* button, const char* filepath, const char* prefix);

// Funkcje pomocnicze dla wyboru plików
const BandConfig* get_band_config(const gchar* dialog_title_suffix);
void update_file_selection(char** target_path_variable, GtkButton* button_to_update,
                          GtkFileChooser* file_chooser, const BandConfig* config);
void handle_file_selection(GtkWindow* parent_window, const gchar* dialog_title_suffix,
                          char** target_path_variable, GtkButton* button_to_update);

// Funkcja pomocnicza do odroczonego niszczenia widgetu
gboolean destroy_widget_idle(gpointer data);

#endif // GUI_UTILS_H