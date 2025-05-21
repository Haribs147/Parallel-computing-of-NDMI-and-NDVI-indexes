#ifndef VISUALIZATION_H
#define VISUALIZATION_H

#include <gdk-pixbuf/gdk-pixbuf.h>
#include "index_calculator.h"

// Structure to hold RGB color values (0-255)
typedef struct {
    unsigned char r;
    unsigned char g;
    unsigned char b;
} RGBColor;

void map_index_value_to_rgb(float value, const char* map_type, unsigned char *r, unsigned char *g, unsigned char *b);

GdkPixbuf* generate_pixbuf_from_index_data(const float* index_data, int width, int height, const char* map_type);

#endif