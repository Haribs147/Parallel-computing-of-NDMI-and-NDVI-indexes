#ifndef UTILS_H
#define UTILS_H

#include <string.h>
#include <sys/time.h>

const char* get_short_filename(const char* filepath);
size_t pixel_index(int x, int y, int width);
const char* detect_band_from_filename(const char* filename);
char* get_timestamp();
double get_time_diff(struct timeval start, struct timeval end);
int clamp(int value, int min, int max);

#endif
