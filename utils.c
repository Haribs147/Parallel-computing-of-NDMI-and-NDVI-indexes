#include "utils.h"
#include <string.h>
#include <stdio.h>

const char* get_short_filename(const char *filepath) {
    if (!filepath) {
        return "(nie wybrano)";
    }

    // Wyszukaj ostatnie wystąpienie separatorów ścieżki
    const char *last_slash = strrchr(filepath, '/');
    const char *last_backslash = strrchr(filepath, '\\');

    // Jeśli oba separatory istnieją, wybierz ten, który jest dalej (bardziej na prawo)
    if (last_slash && last_backslash) {
        return (last_slash > last_backslash) ? last_slash + 1 : last_backslash + 1;
    } else if (last_slash) {
        return last_slash + 1;
    } else if (last_backslash) {
        return last_backslash + 1;
    }
    
    return filepath;
}

// Funkcja obliczająca indeks piksela w obrazie jednowymiarowym
size_t pixel_index(int x, int y, int width) {
    return (size_t)(y * width + x);
}