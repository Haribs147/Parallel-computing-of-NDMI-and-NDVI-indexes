#include "index_calculator.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>   // Dla fabsf
#include <float.h>  // Dla FLT_EPSILON

// Wartości SCL, które oznaczają piksele do wykluczenia z obliczeń wskaźników
// 0=NO_DATA, 1=SATURATED_DEFECTIVE, 3=CLOUD_SHADOW, 6=WATER,
// 8=CLOUD_MEDIUM_PROBABILITY, 9=CLOUD_HIGH_PROBABILITY, 10=THIN_CIRRUS, 11=SNOW_ICE
static const float SCL_EXCLUDE_VALUES[] = {0.0f, 1.0f, 3.0f, 6.0f, 8.0f, 9.0f, 10.0f, 11.0f};
static const int SCL_EXCLUDE_COUNT = sizeof(SCL_EXCLUDE_VALUES) / sizeof(SCL_EXCLUDE_VALUES[0]);

// Funkcja pomocnicza do sprawdzania, czy wartość SCL powinna być zamaskowana
static gboolean is_scl_pixel_masked(float scl_value) {
    for (int i = 0; i < SCL_EXCLUDE_COUNT; ++i) {
        // Porównanie float dla dyskretnych wartości SCL
        if (roundf(scl_value) == roundf(SCL_EXCLUDE_VALUES[i])) {
            return TRUE;
        }
    }
    return FALSE;
}

float* calculate_ndvi(const float* nir_band, const float* red_band,
                      int width, int height,
                      const float* scl_band) {
    if (!nir_band || !red_band || !scl_band || width <= 0 || height <= 0) {
        fprintf(stderr, "Error: Invalid input parameters for calculate_ndvi.\n");
        return NULL;
    }

    size_t num_pixels = (size_t)width * height;
    float* ndvi_data = (float*)malloc(num_pixels * sizeof(float));
    if (!ndvi_data) {
        fprintf(stderr, "Error: Memory allocation failed for NDVI data.\n");
        return NULL;
    }

    for (size_t i = 0; i < num_pixels; ++i) {
        if (is_scl_pixel_masked(scl_band[i])) {
            ndvi_data[i] = INDEX_NO_DATA_VALUE;
            continue;
        }

        float nir = nir_band[i];
        float red = red_band[i];
        float denominator = nir + red;

        if (fabsf(denominator) < FLT_EPSILON) { // Unikanie dzielenia przez zero
            ndvi_data[i] = INDEX_NO_DATA_VALUE; // Lub 0.0f, w zależności od preferencji
        } else {
            float ndvi_val = (nir - red) / denominator;
            // Zaciskanie wartości do zakresu [-1, 1] dla pewności, chociaż teoretycznie powinno się mieścić
            if (ndvi_val < -1.0f) ndvi_val = -1.0f;
            if (ndvi_val > 1.0f) ndvi_val = 1.0f;
            ndvi_data[i] = ndvi_val;
        }
    }
    return ndvi_data;
}

float* calculate_ndmi(const float* nir_band, const float* swir1_band,
                      int width, int height,
                      const float* scl_band) {
    if (!nir_band || !swir1_band || !scl_band || width <= 0 || height <= 0) {
        fprintf(stderr, "Error: Invalid input parameters for calculate_ndmi.\n");
        return NULL;
    }

    size_t num_pixels = (size_t)width * height;
    float* ndmi_data = (float*)malloc(num_pixels * sizeof(float));
    if (!ndmi_data) {
        fprintf(stderr, "Error: Memory allocation failed for NDMI data.\n");
        return NULL;
    }

    for (size_t i = 0; i < num_pixels; ++i) {
        if (is_scl_pixel_masked(scl_band[i])) {
            ndmi_data[i] = INDEX_NO_DATA_VALUE;
            continue;
        }

        float nir = nir_band[i];
        float swir1 = swir1_band[i];
        float denominator = nir + swir1;

        if (fabsf(denominator) < FLT_EPSILON) { // Unikanie dzielenia przez zero
            ndmi_data[i] = INDEX_NO_DATA_VALUE; // Lub 0.0f
        } else {
            float ndmi_val = (nir - swir1) / denominator;
            // Zaciskanie wartości do zakresu [-1, 1]
            if (ndmi_val < -1.0f) ndmi_val = -1.0f;
            if (ndmi_val > 1.0f) ndmi_val = 1.0f;
            ndmi_data[i] = ndmi_val;
        }
    }
    return ndmi_data;
}
