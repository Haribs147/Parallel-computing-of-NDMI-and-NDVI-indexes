#include "utils.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

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

const char* detect_band_from_filename(const char* filename) {
    if (filename == NULL) return "UNKNOWN";

    if (strstr(filename, "B04") || strstr(filename, "_B04_")) return "B04";
    if (strstr(filename, "B08") || strstr(filename, "_B08_")) return "B08";
    if (strstr(filename, "B11") || strstr(filename, "_B11_")) return "B11";
    if (strstr(filename, "SCL") || strstr(filename, "_SCL_")) return "SCL";

    return "UNKNOWN";
}

char* get_timestamp() {
    static char timestamp[64];
    struct timeval tv;
    struct tm* tm_info;

    gettimeofday(&tv, NULL);
    tm_info = localtime(&tv.tv_sec);

    snprintf(timestamp, sizeof(timestamp), "%02d:%02d:%02d.%03ld",
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec,
             tv.tv_usec / 1000);

    return timestamp;
}

double get_time_diff(struct timeval start, struct timeval end) {
    return (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
}