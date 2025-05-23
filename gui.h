#ifndef GUI_H
#define GUI_H

#include <gtk/gtk.h>

int run_gui(int argc, char *argv[]);

typedef struct {
    const char* suffix;
    const char* prefix;
    const char* default_text;
} BandConfig;

static const BandConfig band_configs[] = {
    {"pasma B04", "B04: ", "Wczytaj pasmo B04"},
    {"pasma B08", "B08: ", "Wczytaj pasmo B08"},
    {"pasma B11", "B11: ", "Wczytaj pasmo B11"},
    {"pasma SCL", "SCL: ", "Wczytaj pasmo SCL"},
    {NULL, "Plik: ", "Wybierz plik"} // default
};

#endif
