#include "visualization.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h> // For isnan, isinf if needed, and general math
#include <string.h> // For strcmp

// Nie ma już potrzeby dołączania omp.h

// Helper for linear interpolation between two values
static float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

void map_index_value_to_rgb(float value, const char* map_type /* currently unused for this specific ramp */,
                              unsigned char *r, unsigned char *g, unsigned char *b) {
    if (value == INDEX_NO_DATA_VALUE || isnan(value) || isinf(value)) {
        *r = 0; *g = 0; *b = 0; // Czarny dla NoData
        return;
    }

    // Zaciskanie wartości do [-1, 1] dla pewności
    if (value < -1.0f) value = -1.0f;
    if (value > 1.0f) value = 1.0f;

    // Gradient Czerwony -> Żółty -> Zielony
    // -1.0 ma być czerwony (255, 0, 0)
    //  0.0 ma być żółty  (255, 255, 0)
    // +1.0 ma być zielony (0, 255, 0)

    if (value < 0.0f) { // Od Czerwonego (-1.0) do Żółtego (0.0)
        // t rośnie od 0 (dla value = -1.0) do 1 (dla value = 0.0)
        float t = (value - (-1.0f)) / (0.0f - (-1.0f)); // normalizacja do [0, 1]
        *r = 255;                                      // Czerwony pozostaje na max
        *g = (unsigned char)lerp(0.0f, 255.0f, t);   // Zielony rośnie od 0 do 255
        *b = 0;                                        // Niebieski pozostaje 0
    } else { // Od Żółtego (0.0) do Zielonego (1.0)
        // t rośnie od 0 (dla value = 0.0) do 1 (dla value = 1.0)
        float t = (value - 0.0f) / (1.0f - 0.0f);    // normalizacja do [0, 1]
        *r = (unsigned char)lerp(255.0f, 0.0f, t);   // Czerwony maleje od 255 do 0
        *g = 255;                                      // Zielony pozostaje na max
        *b = 0;                                        // Niebieski pozostaje 0
    }
}

GdkPixbuf* generate_pixbuf_from_index_data(const float* index_data, int width, int height, const char* map_type) {
    if (!index_data || width <= 0 || height <= 0) {
        fprintf(stderr, "Error: Invalid input for generate_pixbuf_from_index_data.\n");
        return NULL;
    }

    GdkPixbuf *pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, width, height);
    if (!pixbuf) {
        fprintf(stderr, "Error: Failed to create GdkPixbuf.\n");
        return NULL;
    }

    guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);
    int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    int n_channels = gdk_pixbuf_get_n_channels(pixbuf);

    // Usunięto #pragma omp parallel for
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            size_t pixel_idx_float = (size_t)y * width + x;
            guchar *p = pixels + (size_t)y * rowstride + (size_t)x * n_channels;
            // map_type jest przekazywany, ale obecna funkcja map_index_value_to_rgb go ignoruje
            // jeśli chcesz różne rampy dla NDVI/NDMI, musisz zmodyfikować map_index_value_to_rgb
            map_index_value_to_rgb(index_data[pixel_idx_float], map_type, &p[0], &p[1], &p[2]);
        }
    }
    return pixbuf;
}