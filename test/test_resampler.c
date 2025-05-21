#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
            printf("%.1f", matrix[i * width + j]);
            if (j < width - 1) printf(", ");
        }
        printf("]\n");
    }
    printf("\n");
}

void test_resample(int input_width, int input_height, int output_width, int output_height,
                  const char* test_name, const char* method) {
    printf("\n===== TEST: %s =====\n", test_name);
    printf("Resample from %dx%d to %dx%d using %s method\n\n",
           input_width, input_height, output_width, output_height, method);

    float* input_matrix = create_test_matrix(input_width, input_height);
    if (input_matrix == NULL) {
        return;
    }

    print_matrix(input_matrix, input_width, input_height, "Input Matrix");

    float* output_matrix = NULL;

    // Wybór metody resamplingu na podstawie argumentu
    if (strcmp(method, "nearest") == 0) {
        output_matrix = nearest_neighbor_resample_scl(
            input_matrix, input_width, input_height, output_width, output_height);
    } else if (strcmp(method, "bilinear") == 0) {
        output_matrix = bilinear_resample_float(
            input_matrix, input_width, input_height, output_width, output_height);
    } else {
        fprintf(stderr, "Error: Unknown resampling method '%s'\n", method);
        free(input_matrix);
        return;
    }

    if (output_matrix == NULL) {
        fprintf(stderr, "Error: Resampling failed\n");
        free(input_matrix);
        return;
    }

    char output_label[100];
    snprintf(output_label, sizeof(output_label), "Output Matrix (%s)",
             strcmp(method, "nearest") == 0 ? "Nearest Neighbor" : "Bilinear");

    print_matrix(output_matrix, output_width, output_height, output_label);

    free(input_matrix);
    free(output_matrix);
}

void print_usage(const char* program_name) {
    printf("Usage: %s <method>\n", program_name);
    printf("Available methods:\n");
    printf("  nearest  - Use Nearest Neighbor resampling\n");
    printf("  bilinear - Use Bilinear resampling\n");
}

int main(int argc, char* argv[]) {
    // Sprawdzenie argumentów
    if (argc != 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char* method = argv[1];

    // Walidacja metody
    if (strcmp(method, "nearest") != 0 && strcmp(method, "bilinear") != 0) {
        fprintf(stderr, "Error: Unknown method '%s'\n", method);
        print_usage(argv[0]);
        return 1;
    }

    printf("Testing %s Resampling\n",
           strcmp(method, "nearest") == 0 ? "Nearest Neighbor" : "Bilinear");

    // Test 1: Macierz 2x2 do 4x4 (powiększenie)
    test_resample(2, 2, 4, 4, "2x2 to 4x4 (enlarge)", method);

    // Test 2: Macierz 4x4 do 2x2 (zmniejszenie)
    test_resample(4, 4, 2, 2, "4x4 to 2x2 (shrink)", method);

    // Test 3: Macierz 3x3 do 6x6 (powiększenie)
    test_resample(3, 3, 6, 6, "3x3 to 6x6 (enlarge)", method);

    // Test 4: Macierz 4x4 do 8x8 (powiększenie)
    test_resample(4, 4, 8, 8, "4x4 to 8x8 (enlarge)", method);

    // Test 5: Macierz 10x1 do 20x2 (powiększenie "płaskiej" macierzy)
    test_resample(10, 1, 20, 2, "10x1 to 20x2 (enlarge flat matrix)", method);

    // Test 6: Macierz 1x10 do 2x20 (powiększenie "wąskiej" macierzy)
    test_resample(1, 10, 2, 20, "1x10 to 2x20 (enlarge narrow matrix)", method);

    // Test 7: Macierz 10x10 do 5x5 (zmniejszenie o połowę)
    test_resample(10, 10, 5, 5, "10x10 to 5x5 (shrink by half)", method);

    // Test 8: Macierz 10x10 do 7x7 (zmniejszenie o nietypowy współczynnik)
    test_resample(10, 10, 7, 7, "10x10 to 7x7 (shrink by non-integer factor)", method);

    return 0;
}

// Kompilacja: gcc -o test_resampler test_resampler.c ../resampler.c -lm
// Użycie: ./test_resampler nearest lub ./test_resampler bilinear