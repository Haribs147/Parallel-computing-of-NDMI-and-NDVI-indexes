#include <stdio.h>
#include <stdlib.h>
#include <math.h> // Dla roundf, jeśli byłby używany, lub innych funkcji matematycznych

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

    // Użyj size_t dla wyniku mnożenia, aby uniknąć przepełnienia przy dużych obrazach
    size_t num_output_pixels = (size_t)output_width * output_height;
    if (num_output_pixels == 0 && (output_width > 0 || output_height > 0) ) { // zabezpieczenie przed przepełnieniem iloczynu jeśli jeden wymiar jest duży a drugi 0
         fprintf(stderr, "Error: Output dimensions result in zero pixels but dimensions are non-zero (potential overflow or invalid args) for nearest_neighbor_resample_scl.\n");
        return NULL;
    }
    if (num_output_pixels > (SIZE_MAX / sizeof(float))) { // Sprawdzenie czy malloc nie dostanie zbyt dużej wartości
        fprintf(stderr, "Error: Requested memory allocation size is too large for nearest_neighbor_resample_scl.\n");
        return NULL;
    }


    float* output_band = (float*)malloc(num_output_pixels * sizeof(float));
    if (output_band == NULL && num_output_pixels > 0) { // Sprawdź num_output_pixels > 0 bo malloc(0) może zwrócić NULL lub unikalny wskaźnik
        fprintf(stderr, "Error: Memory allocation failed for output band in nearest_neighbor_resample_scl.\n");
        return NULL;
    } else if (num_output_pixels == 0) {
        // Jeśli wymiary wyjściowe są 0xN lub Nx0, zwracamy pusty (ale poprawny) bufor lub NULL.
        // GTK i inne biblioteki mogą oczekiwać poprawnego wskaźnika nawet dla danych o zerowym rozmiarze.
        // malloc(0) jest zdefiniowany przez implementację, więc bezpieczniej jest zwrócić NULL lub specjalnie obsłużyć.
        // Tutaj, jeśli output_width lub output_height było 0, num_output_pixels będzie 0.
        // Zwrócenie output_band (który może być NULL z malloc(0) lub małym wskaźnikiem) jest OK.
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
            if (x_in < 0) { // Powinno być >=0 jeśli x_out, x_ratio są dodatnie
                x_in = 0;
            }
            if (y_in >= input_height) {
                y_in = input_height - 1;
            }
            if (y_in < 0) { // Powinno być >=0
                y_in = 0;
            }
            
            // Upewnij się, że x_in i y_in są zawsze w zakresie po zacisnięciu,
            // zwłaszcza jeśli input_width/height jest 1.
            if (input_width > 0) { // Unikaj modulo przez zero
                 x_in = (x_in < 0) ? 0 : (x_in >= input_width ? input_width -1 : x_in);
            } else { // input_width == 0 (lub <=0 co jest błędem obsłużonym na początku)
                // Jeśli input_width jest 0, nie ma skąd brać danych.
                // To powinno być obsłużone przez walidację na początku funkcji.
                // Jeśli jakimś cudem tu dotrzemy, przypisz wartość domyślną lub obsłuż błąd.
                // output_band[y_out * output_width + x_out] = 0.0f; // Przykładowo
                // continue;
            }

            if (input_height > 0) {
                y_in = (y_in < 0) ? 0 : (y_in >= input_height ? input_height -1 : y_in);
            } else {
                // output_band[y_out * output_width + x_out] = 0.0f;
                // continue;
            }


            // Jeśli input_width lub input_height jest 0, to pętle nie powinny się wykonać
            // lub błąd powinien być zwrócony na początku.
            // Zakładając, że input_width i input_height > 0 (sprawdzone na początku)
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