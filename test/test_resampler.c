#include <stdio.h>
#include <stdlib.h>
#include "../resampler.h"

float* create_test_matrix(int width, int height) {
    float* matrix = malloc(width * height * sizeof(float));
    if (matrix == NULL) {
        fprintf(stderr, "Error: Failed to allocate memory for test matrix\n");
        return NULL;
    }

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            matrix[i * width + j] = (float)((i * width + j) % 90 + 10); // Wartości 10-99
        }
    }

    return matrix;
}

void print_matrix(const float* matrix, int width, int height, const char* label) {
    printf("%s (%dx%d):\n", label, width, height);

    for (int i = 0; i < height; i++) {
        printf("[");
        for (int j = 0; j < width; j++) {
            printf("%.0f", matrix[i * width + j]);
            if (j < width - 1) printf(", ");
        }
        printf("]\n");
    }
    printf("\n");
}

void test_resample(int input_width, int input_height, int output_width, int output_height, const char* test_name) {
    printf("\n===== TEST: %s =====\n", test_name);
    printf("Resample from %dx%d to %dx%d\n\n", input_width, input_height, output_width, output_height);

    float* input_matrix = create_test_matrix(input_width, input_height);
    if (input_matrix == NULL) {
        return;
    }

    print_matrix(input_matrix, input_width, input_height, "Input Matrix");

    float* output_matrix = nearest_neighbor_resample_scl(
        input_matrix, input_width, input_height, output_width, output_height);

    if (output_matrix == NULL) {
        fprintf(stderr, "Error: Resampling failed\n");
        free(input_matrix);
        return;
    }

    print_matrix(output_matrix, output_width, output_height, "Output Matrix (Nearest Neighbor)");

    free(input_matrix);
    free(output_matrix);
}

int main() {
    printf("Testing Nearest Neighbor Resampling\n");

    // Test 1: Macierz 2x2 do 4x4 (powiększenie)
    test_resample(2, 2, 4, 4, "2x2 to 4x4 (enlarge)");

    // Test 2: Macierz 4x4 do 2x2 (zmniejszenie)
    test_resample(4, 4, 2, 2, "4x4 to 2x2 (shrink)");

    // Test 3: Macierz 3x3 do 6x6 (powiększenie)
    test_resample(3, 3, 6, 6, "3x3 to 6x6 (enlarge)");

    // Test 4: Macierz 4x4 do 8x8 (powiększenie)
    test_resample(4, 4, 8, 8, "4x4 to 8x8 (enlarge)");

    // Test 5: Macierz 10x1 do 20x2 (powiększenie "płaskiej" macierzy)
    test_resample(10, 1, 20, 2, "10x1 to 20x2 (enlarge flat matrix)");

    // Test 6: Macierz 1x10 do 2x20 (powiększenie "wąskiej" macierzy)
    test_resample(1, 10, 2, 20, "1x10 to 2x20 (enlarge narrow matrix)");

    // Test 7: Macierz 10x10 do 5x5 (zmniejszenie o połowę)
    test_resample(10, 10, 5, 5, "10x10 to 5x5 (shrink by half)");

    // Test 8: Macierz 10x10 do 7x7 (zmniejszenie o nietypowy współczynnik)
    test_resample(10, 10, 7, 7, "10x10 to 7x7 (shrink by non-integer factor)");

    return 0;
}

// TODO dodać do makefile
// Kompilacja: gcc -o test_resampler test_resampler.c ../resampler.c -lm