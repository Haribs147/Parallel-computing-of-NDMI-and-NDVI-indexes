#include "visualization.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "utils.h"

// Linear interpolation
static float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

static GdkPixbuf* create_image_display_buffer(const float* index_data, int width, int height) {
    if (!index_data || width <= 0 || height <= 0) {
        fprintf(stderr, "Error: Invalid input parameters for creating image buffer.\n");
        return NULL;
    }

    GdkPixbuf *pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, width, height);
    if (!pixbuf) {
        fprintf(stderr, "Error: Failed to create GdkPixbuf for image display.\n");
        return NULL;
    }
    return pixbuf;
}

// Calculates the memory address of a specific pixel (x,y) in a raw pixel buffer.
static guchar* get_pixel_pointer(guchar *pixels, int y, int x, int rowstride, int n_channels) {
    return pixels + (size_t)y * rowstride + (size_t)x * n_channels;
}

void map_index_value_to_rgb(float value, unsigned char *r,
                                         unsigned char *g, unsigned char *b) {
    if (value == INDEX_NO_DATA_VALUE || isnan(value) || isinf(value)) {
        *r = 0; *g = 0; *b = 0;
        return;
    }

    if (value < 0.0f) {
        // t rośnie od 0 (dla value = -1.0) do 1 (dla value = 0.0)
        float t = (value - (-1.0f)) / (0.0f - (-1.0f));
        *r = 255;
        *g = (unsigned char)lerp(0.0f, 255.0f, t);
        *b = 0;
    } else {
        // t rośnie od 0 (dla value = 0.0) do 1 (dla value = 1.0)
        float t = (value - 0.0f) / (1.0f - 0.0f);
        *r = (unsigned char)lerp(255.0f, 0.0f, t);// essa
        *g = 255;
        *b = 0;
    }
}

GdkPixbuf* generate_pixbuf_from_index_data(const float* index_data, int width, int height) {
    GdkPixbuf *pixbuf = create_image_display_buffer(index_data, width, height);
    if (!pixbuf) {
        return NULL;
    }

    guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);
    int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    int n_channels = gdk_pixbuf_get_n_channels(pixbuf);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            size_t pixel_idx_float = pixel_index(x, y, width);
            guchar *p = get_pixel_pointer(pixels, y, x, rowstride, n_channels); 
            map_index_value_to_rgb(index_data[pixel_idx_float], &p[0], &p[1], &p[2]);
        }
    }
    return pixbuf;
}