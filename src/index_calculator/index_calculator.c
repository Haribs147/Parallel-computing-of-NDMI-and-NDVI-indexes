#include "index_calculator.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

#include "../utils/utils.h"

/**
 * @brief Tablica lookup (SCL_EXCLUDE_LOOKUP) do szybkiego sprawdzania wykluczeń SCL.
 *
 * Ta tablica ma 12 elementów, odpowiadających wartościom SCL od 0 do 11.
 * Jeśli SCL_EXCLUDE_LOOKUP[wartosc_scl] jest TRUE, oznacza to, że piksel
 * z daną wartością SCL powinien być wykluczony z obliczeń.
 *
 * Wykluczane wartości:
 * - 0:  NO_DATA
 * - 1:  SATURATED_DEFECTIVE
 * - 3:  CLOUD_SHADOW
 * - 6:  WATER
 * - 8:  CLOUD_MEDIUM_PROBABILITY
 * - 9:  CLOUD_HIGH_PROBABILITY
 * - 10: THIN_CIRRUS
 * - 11: SNOW_ICE
 *
 * Niewykluczane wartości:
 * - 2:  DARK_AREA_PIXELS
 * - 4:  VEGETATION
 * - 5:  NOT_VEGETATED
 * - 7:  UNCLASSIFIED
 */
static const gboolean SCL_EXCLUDE_LOOKUP[12] = {
    TRUE, // SCL 0: NO_DATA
    TRUE, // SCL 1: SATURATED_DEFECTIVE
    FALSE, // SCL 2: DARK_AREA_PIXELS
    TRUE, // SCL 3: CLOUD_SHADOW
    FALSE, // SCL 4: VEGETATION
    FALSE, // SCL 5: NOT_VEGETATED
    TRUE, // SCL 6: WATER
    FALSE, // SCL 7: UNCLASSIFIED
    TRUE, // SCL 8: CLOUD_MEDIUM_PROBABILITY
    TRUE, // SCL 9: CLOUD_HIGH_PROBABILITY
    TRUE, // SCL 10: THIN_CIRRUS
    TRUE // SCL 11: SNOW_ICE
};

// Funkcja pomocnicza do sprawdzania, czy wartość SCL powinna być zamaskowana
static gboolean is_scl_pixel_masked(float scl_value)
{
    int index = (int)roundf(scl_value);
    if (index >= 0 && index < 12)
    {
        return SCL_EXCLUDE_LOOKUP[index];
    }
    return TRUE;
}

/**
 * @brief Alokuje pamięć na dane wskaźnika.
 *
 * @param width Szerokość obrazu.
 * @param height Wysokość obrazu.
 * @param index_name Nazwa wskaźnika (do komunikatów o błędach).
 * @return Wskaźnik do zaalokowanej pamięci lub NULL w przypadku błędu.
 */
static float* allocate_index_data(int width, int height, const char* index_name)
{
    size_t num_pixels = (size_t)width * height;
    float* data = (float*)malloc(num_pixels * sizeof(float));
    if (!data)
    {
        fprintf(stderr, "Error: Memory allocation failed for %s data.\n", index_name);
        return NULL;
    }
    return data;
}

/**
 * @brief Oblicza znormalizowaną różnicę (A - B) / (A + B).
 * Zawiera sprawdzanie dzielenia przez zero i zaciskanie wyniku.
 *
 * @param band_a Wartość piksela pasma A.
 * @param band_b Wartość piksela pasma B.
 * @return Wartość wskaźnika lub INDEX_NO_DATA_VALUE.
 */
static float calculate_normalized_difference(float band_a, float band_b)
{
    float denominator = band_a + band_b;
    if (fabsf(denominator) < FLT_EPSILON)
    {
        return INDEX_NO_DATA_VALUE;
    }
    float index_val = (band_a - band_b) / denominator;
    // Zaciskanie wartości do zakresu [-1, 1]
    if (index_val < -1.0f) return -1.0f;
    if (index_val > 1.0f) return 1.0f;
    return index_val;
}


float* calculate_index_base(const float* band_a, const float* band_b,
                            int width, int height,
                            const float* scl_band,
                            const char* index_name)
{
    struct timeval start_time, end_time;
    double elapsed_time;
    gettimeofday(&start_time, NULL);
    g_print("[%s] Rozpoczynanie obliczania %s.\n", get_timestamp(), index_name);

    if (!band_a || !band_b || !scl_band || width <= 0 || height <= 0)
    {
        fprintf(stderr, "Error: Invalid input parameters for calculate_%s.\n", index_name);
        return NULL;
    }

    float* result_data = allocate_index_data(width, height, index_name);
    if (!result_data)
    {
        return NULL;
    }

    size_t num_pixels = (size_t)width * height;

    #pragma omp parallel for shared(band_a, band_b, scl_band, result_data)
    for (size_t i = 0; i < num_pixels; i++)
    {
        if (is_scl_pixel_masked(scl_band[i]))
        {
            result_data[i] = INDEX_NO_DATA_VALUE;
            continue;
        }
        result_data[i] = calculate_normalized_difference(band_a[i], band_b[i]);
    }

    gettimeofday(&end_time, NULL);
    elapsed_time = get_time_diff(start_time, end_time);
    g_print("[%s] Zakończono obliczanie %s (czas: %.2fs)\n", get_timestamp(), index_name, elapsed_time);

    return result_data;
}

float* calculate_ndvi(const float* nir_band, const float* red_band,
                      int width, int height,
                      const float* scl_band)
{
    return calculate_index_base(nir_band, red_band, width, height, scl_band, "NDVI");
}

float* calculate_ndmi(const float* nir_band, const float* swir1_band,
                      int width, int height,
                      const float* scl_band)
{
    return calculate_index_base(nir_band, swir1_band, width, height, scl_band, "NDMI");
}
