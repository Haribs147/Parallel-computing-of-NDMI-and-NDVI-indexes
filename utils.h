#ifndef UTILS_H
#define UTILS_H

#include <string.h>

const char* get_short_filename(const char *filepath);

size_t pixel_index(int x, int y, int width);

#endif