#ifndef VISUALIZATION_H
#define VISUALIZATION_H

#include <gdk-pixbuf/gdk-pixbuf.h>
#include "../index_calculator/index_calculator.h"

void map_index_value_to_rgb(float value, unsigned char* r, unsigned char* g, unsigned char* b);

GdkPixbuf* generate_pixbuf_from_index_data(const float* index_data, int width, int height);

#endif
