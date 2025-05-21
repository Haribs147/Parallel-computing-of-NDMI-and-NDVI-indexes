#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "resampler.h"

float* nearest_neighbor_resample_scl(
    const float* input_band,
    int input_width,
    int input_height,
    int output_width,
    int output_height
) {
    if (input_band == NULL || input_width <= 0 || input_height <= 0 || output_width <= 0 || output_height <= 0) {
        fprintf(stderr, "Error: Invalid input parameters for nearest_neighbor_resample_scl.\n");
        return NULL;
    }

    size_t num_output_pixels = (size_t)output_width * output_height;

    if (num_output_pixels == 0){
         fprintf(stderr, "Error: Output dimensions result in zero pixels but dimensions are non-zero (potential overflow or invalid args) for nearest_neighbor_resample_scl.\n");
        return NULL;
    }

    // Sprawdzenie, czy malloc nie dostanie zbyt dużej wartości
    if (num_output_pixels > (SIZE_MAX / sizeof(float))) {
        fprintf(stderr, "Error: Requested memory allocation size is too large for nearest_neighbor_resample_scl.\n");
        return NULL;
    }


    float* output_band = malloc(num_output_pixels * sizeof(float));
    if (output_band == NULL) {
        fprintf(stderr, "Error: Memory allocation failed for output band in nearest_neighbor_resample_scl.\n");
        return NULL;
    }

    float x_ratio = (float)input_width / output_width;
    float y_ratio = (float)input_height / output_height;

    for (int y_out = 0; y_out < output_height; y_out++) {
        for (int x_out = 0; x_out < output_width; x_out++) {
            // Standardowe mapowanie z zaokrągleniem do najbliższego sąsiada
            int x_in = (int)(x_out * x_ratio + 0.5f);
            int y_in = (int)(y_out * y_ratio + 0.5f);

            // Zaciskanie współrzędnych do granic obrazu wejściowego
            if (x_in >= input_width) {
                x_in = input_width - 1;
            }
            if (x_in < 0) {
                x_in = 0;
            }
            if (y_in >= input_height) {
                y_in = input_height - 1;
            }
            if (y_in < 0) {
                y_in = 0;
            }

            output_band[y_out * output_width + x_out] = input_band[y_in * input_width + x_in];
        }
    }
    return output_band;
}

float* bilinear_resample_float(
    const float* input_band,
    int input_width,
    int input_height,
    int output_width,
    int output_height
) {
    if (input_band == NULL || input_width <= 0 || input_height <= 0 || output_width <= 0 || output_height <= 0) {
        fprintf(stderr, "Error: Invalid input parameters for bilinear_resample_float.\n");
        return NULL;
    }

    size_t num_output_pixels = (size_t)output_width * output_height;
    if (num_output_pixels == 0 && (output_width > 0 || output_height > 0) ) {
        fprintf(stderr, "Error: Output dimensions result in zero pixels but dimensions are non-zero (potential overflow or invalid args) for bilinear_resample_float.\n");
        return NULL;
    }
    if (num_output_pixels > (SIZE_MAX / sizeof(float))) { // Sprawdzenie czy malloc nie dostanie zbyt dużej wartości
        fprintf(stderr, "Error: Requested memory allocation size is too large for bilinear_resample_float.\n");
        return NULL;
    }

    float* output_band = (float*)malloc(num_output_pixels * sizeof(float));
    if (output_band == NULL && num_output_pixels > 0) {
        fprintf(stderr, "Error: Memory allocation failed for output band in bilinear_resample_float.\n");
        return NULL;
    } else if (num_output_pixels == 0) {
        // Zwrócenie output_band (który może być NULL z malloc(0) lub małym wskaźnikiem) jest OK.
        // Funkcja powinna zwrócić poprawny, choć potencjalnie pusty, bufor.
    }


    float x_ratio = (float)input_width / output_width;
    float y_ratio = (float)input_height / output_height;

    for (int y_out = 0; y_out < output_height; y_out++) {
        for (int x_out = 0; x_out < output_width; x_out++) {
            float x_in_proj = (x_out + 0.5f) * x_ratio - 0.5f; // mapowanie środka piksela wyjściowego na siatkę wejściową
            float y_in_proj = (y_out + 0.5f) * y_ratio - 0.5f;

            int x1 = (int)floorf(x_in_proj);
            int y1 = (int)floorf(y_in_proj);

            // Współrzędne x2, y2
            int x2 = x1 + 1;
            int y2 = y1 + 1;

            // Ułamkowe odległości
            float dx = x_in_proj - (float)x1;
            float dy = y_in_proj - (float)y1;

            // Zabezpieczenie granic dla x1, y1, x2, y2
            if (x1 < 0) x1 = 0;
            if (x1 >= input_width) x1 = input_width - 1;
            if (y1 < 0) y1 = 0;
            if (y1 >= input_height) y1 = input_height - 1;

            if (x2 < 0) x2 = 0; // Powinno być rzadkie, jeśli x1 jest >=0
            if (x2 >= input_width) x2 = input_width - 1;
            if (y2 < 0) y2 = 0; // Powinno być rzadkie, jeśli y1 jest >=0
            if (y2 >= input_height) y2 = input_height - 1;


            // Wartości pikseli otaczających
            float p11 = input_band[y1 * input_width + x1];
            float p21 = input_band[y1 * input_width + x2]; // x2, y1
            float p12 = input_band[y2 * input_width + x1]; // x1, y2
            float p22 = input_band[y2 * input_width + x2];

            // Interpolacja dwuliniowa
            float interpolated_value =
                p11 * (1.0f - dx) * (1.0f - dy) +
                p21 * dx * (1.0f - dy) +
                p12 * (1.0f - dx) * dy +
                p22 * dx * dy;

            output_band[y_out * output_width + x_out] = interpolated_value;
        }
    }
    return output_band;
}

float* average_resample_float(
    const float* input_band,
    int input_width,
    int input_height,
    int output_width,
    int output_height
) {
    if (input_band == NULL || input_width <= 0 || input_height <= 0 || output_width <= 0 || output_height <= 0) {
        fprintf(stderr, "Error: Invalid input parameters for average_resample_float.\n");
        return NULL;
    }
    // Ta funkcja jest przeznaczona do downsamplingu, np. o współczynnik 2
    // Sprawdźmy, czy wymiary wyjściowe są mniej więcej połową wejściowych
    // To uproszczone sprawdzenie, bardziej rygorystyczne mogłoby weryfikować dokładny współczynnik.
    if (output_width > input_width || output_height > input_height) {
        fprintf(stderr, "Warning: average_resample_float called with output dimensions larger than input. This is for downsampling.\n");
        // Można by zwrócić NULL lub próbować kontynuować, ale to nietypowe użycie.
    }

    size_t num_output_pixels = (size_t)output_width * output_height;
     if (num_output_pixels == 0 && (output_width > 0 || output_height > 0) ) {
        fprintf(stderr, "Error: Output dimensions result in zero pixels but dimensions are non-zero (potential overflow or invalid args) for average_resample_float.\n");
        return NULL;
    }
    if (num_output_pixels > (SIZE_MAX / sizeof(float))) {
        fprintf(stderr, "Error: Requested memory allocation size is too large for average_resample_float.\n");
        return NULL;
    }

    float* output_band = (float*)malloc(num_output_pixels * sizeof(float));
    if (output_band == NULL && num_output_pixels > 0) {
        fprintf(stderr, "Error: Memory allocation failed for output band in average_resample_float.\n");
        return NULL;
    } else if (num_output_pixels == 0) {
        return output_band; // Może być NULL lub mały wskaźnik, ale jest poprawny dla 0 pikseli
    }

    // Współczynniki skalowania (ile pikseli wejściowych przypada na jeden piksel wyjściowy)
    // Dla downsamplingu 10m -> 20m, ratio będzie około 2.0
    float x_scale_factor = (float)input_width / output_width;
    float y_scale_factor = (float)input_height / output_height;

    for (int y_out = 0; y_out < output_height; y_out++) {
        for (int x_out = 0; x_out < output_width; x_out++) {
            // Określ zakres pikseli wejściowych dla danego piksela wyjściowego
            // Początek bloku (lewy górny róg)
            int x_start_in = (int)roundf(x_out * x_scale_factor);
            int y_start_in = (int)roundf(y_out * y_scale_factor);
            // Koniec bloku (prawy dolny róg + 1, aby objąć piksele do uśrednienia)
            int x_end_in = (int)roundf((x_out + 1) * x_scale_factor);
            int y_end_in = (int)roundf((y_out + 1) * y_scale_factor);

            // Zaciskanie do granic obrazu wejściowego
            if (x_start_in < 0) x_start_in = 0;
            if (x_start_in >= input_width) x_start_in = input_width -1; // Powinno dać 0 pikseli do sumowania
            if (y_start_in < 0) y_start_in = 0;
            if (y_start_in >= input_height) y_start_in = input_height -1;

            if (x_end_in <= x_start_in) x_end_in = x_start_in + 1; // Upewnij się, że jest co najmniej 1 piksel
            if (x_end_in > input_width) x_end_in = input_width;
            if (y_end_in <= y_start_in) y_end_in = y_start_in + 1;
            if (y_end_in > input_height) y_end_in = input_height;

            float sum = 0.0f;
            int count = 0;

            for (int y_in = y_start_in; y_in < y_end_in; y_in++) {
                for (int x_in = x_start_in; x_in < x_end_in; x_in++) {
                    sum += input_band[y_in * input_width + x_in];
                    count++;
                }
            }

            if (count > 0) {
                output_band[y_out * output_width + x_out] = sum / count;
            } else {
                // Sytuacja awaryjna, nie powinno się zdarzyć przy poprawnym zaciskaniu
                // Można użyć najbliższego sąsiada jako fallback lub wartości NoData
                int center_x_in = (x_start_in + x_end_in -1) / 2;
                int center_y_in = (y_start_in + y_end_in -1) / 2;
                 if (center_x_in < 0) center_x_in = 0;
                 if (center_x_in >= input_width) center_x_in = input_width - 1;
                 if (center_y_in < 0) center_y_in = 0;
                 if (center_y_in >= input_height) center_y_in = input_height - 1;
                output_band[y_out * output_width + x_out] = input_band[center_y_in * input_width + center_x_in];
                // Alternatywnie: output_band[y_out * output_width + x_out] = 0.0f; // lub wartość NoData
            }
        }
    }
    return output_band;
}
