#ifndef INDEX_CALCULATOR_H
#define INDEX_CALCULATOR_H

#include <gtk/gtk.h>

#define INDEX_NO_DATA_VALUE -2.0f

float* calculate_ndvi(const float* nir_band, const float* red_band,
                      int width, int height,
                      const float* scl_band);

float* calculate_ndmi(const float* nir_band, const float* swir1_band,
                      int width, int height,
                      const float* scl_band);

#endif
