#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "resampler.h"
#include "utils.h"

typedef struct
{
    int target_width;
    int target_height;
    gboolean is_10m_resolution;
} ResamplingParams;

// ====== WALIDACJA ======
int validate_input_params(const float* input_band, int input_width, int input_height, int output_width, int output_height);
int validate_output_size(int output_width, int output_height, size_t* num_pixels);
// ====== PAMIĘĆ ======
float* allocate_output_band(size_t num_pixels);
float* prepare_data(const float* input_band, int input_width, int input_height, int output_width, int output_height, const char* error_suffix);
void replace_band_data(BandData* band_data, float* new_data);
// ====== FUNKCJONALNOŚĆ ======
int resample_single_band(BandData* band_data, int band_index, const ResamplingParams* params);
// ====== ALGORYTMY RESAMPLINGU ======
void perform_nearest_neighbor_resample(const float* input_band, float* output_band, int input_width, int input_height, int output_width, int output_height);
float* nearest_neighbor_resample_scl(const float* input_band, int input_width, int input_height, int output_width, int output_height);
void perform_bilinear_resample(const float* input_band, float* output_band, int input_width, int input_height, int output_width, int output_height);
float* bilinear_resample_float(const float* input_band, int input_width, int input_height, int output_width, int output_height);
void perform_average_resample(const float* input_band, float* output_band, int input_width, int input_height, int output_width, int output_height);
float* average_resample_float(const float* input_band, int input_width, int input_height, int output_width, int output_height);


int resample_all_bands_to_target_resolution(BandData* bands, int band_count, gboolean target_resolution_10m)
{
    // Przygotuj parametry resamplingu
    ResamplingParams params;
    params.is_10m_resolution = target_resolution_10m;

    if (target_resolution_10m)
    {
        // Docelowa rozdzielczość 10m - używaj wymiarów B04
        params.target_width = *bands[B04].width;
        params.target_height = *bands[B04].height;
    }
    else
    {
        // Docelowa rozdzielczość 20m - używaj wymiarów B11
        params.target_width = *bands[B11].width;
        params.target_height = *bands[B11].height;
    }

    g_print("[%s] Rozpoczynanie resamplingu do rozdzielczości %s.\n",
            get_timestamp(), target_resolution_10m ? "10m" : "20m");

    // Wykonaj resampling dla każdego pasma
    for (int i = 0; i < band_count; i++)
    {
        if (resample_single_band(&bands[i], i, &params) != 0)
        {
            fprintf(stderr, "Błąd podczas resamplingu pasma %s.\n", bands[i].band_name);
            return -1;
        }
    }

    g_print("[%s] Resampling zakończony pomyślnie.\n", get_timestamp());
    return 0;
}

int validate_input_params(const float* input_band, int input_width, int input_height,
                          int output_width, int output_height)
{
    if (input_band == NULL || input_width <= 0 || input_height <= 0 ||
        output_width <= 0 || output_height <= 0)
    {
        fprintf(stderr, "Error: Invalid input parameters");
        return 0;
    }
    return 1;
}

int validate_output_size(int output_width, int output_height, size_t* num_pixels)
{
    *num_pixels = (size_t)output_width * output_height;

    if (*num_pixels == 0)
    {
        fprintf(
            stderr,
            "Error: Output dimensions result in zero pixels but dimensions are non-zero (potential overflow or invalid args)");
        return 0;
    }

    // Sprawdzenie, czy malloc nie dostanie zbyt dużej wartości
    if (*num_pixels > (SIZE_MAX / sizeof(float)))
    {
        fprintf(stderr, "Error: Requested memory allocation size is too large");
        return 0;
    }

    return 1;
}

float* allocate_output_band(size_t num_pixels)
{
    float* output_band = malloc(num_pixels * sizeof(float));
    if (output_band == NULL)
    {
        fprintf(stderr, "Error: Memory allocation failed for output band");
        return NULL;
    }
    return output_band;
}

float* prepare_data(
    const float* input_band,
    int input_width,
    int input_height,
    int output_width,
    int output_height,
    const char* error_suffix
)
{
    if (!validate_input_params(input_band, input_width, input_height, output_width, output_height))
    {
        fprintf(stderr, error_suffix);
        return NULL;
    }

    size_t num_output_pixels;
    if (!validate_output_size(output_width, output_height, &num_output_pixels))
    {
        fprintf(stderr, error_suffix);
        return NULL;
    }

    float* output_band = allocate_output_band(num_output_pixels);
    if (output_band == NULL)
    {
        fprintf(stderr, error_suffix);
        return NULL;
    }

    return output_band;
}

void perform_nearest_neighbor_resample(const float* input_band, float* output_band,
                                       int input_width, int input_height,
                                       int output_width, int output_height)
{
    float x_ratio = (float)input_width / output_width;
    float y_ratio = (float)input_height / output_height;

    for (int y_out = 0; y_out < output_height; y_out++)
    {
        for (int x_out = 0; x_out < output_width; x_out++)
        {
            // Standardowe mapowanie z zaokrągleniem do najbliższego sąsiada
            int x_in = (int)(x_out * x_ratio + 0.5f);
            int y_in = (int)(y_out * y_ratio + 0.5f);

            // Zaciskanie współrzędnych do granic obrazu wejściowego
            x_in = clamp(x_in, 0, input_width - 1);
            y_in = clamp(y_in, 0, input_height - 1);

            output_band[pixel_index(x_out, y_out, output_width)] =
                input_band[pixel_index(x_in, y_in, input_width)];
        }
    }
}

float* nearest_neighbor_resample_scl(
    const float* input_band,
    int input_width,
    int input_height,
    int output_width,
    int output_height
)
{
    char* error_suffix = "in nearest_neighbor_resample_scl.\n";

    float* output_band = prepare_data(
        input_band, input_width, input_height, output_width, output_height, error_suffix);

    if (output_band == NULL)
    {
        return NULL;
    }

    perform_nearest_neighbor_resample(input_band, output_band,
                                      input_width, input_height,
                                      output_width, output_height);

    return output_band;
}

void perform_bilinear_resample(const float* input_band, float* output_band,
                               int input_width, int input_height,
                               int output_width, int output_height)
{
    float x_ratio = (float)input_width / output_width;
    float y_ratio = (float)input_height / output_height;

    for (int y_out = 0; y_out < output_height; y_out++)
    {
        for (int x_out = 0; x_out < output_width; x_out++)
        {
            // Mapowanie środka piksela wyjściowego na siatkę wejściową
            float x_in_proj = (x_out + 0.5f) * x_ratio - 0.5f;
            float y_in_proj = (y_out + 0.5f) * y_ratio - 0.5f;

            // Piksele graniczne
            int x1 = (int)floorf(x_in_proj);
            int y1 = (int)floorf(y_in_proj);
            int x2 = x1 + 1;
            int y2 = y1 + 1;

            // Ułamkowe odległości
            float dx = x_in_proj - (float)x1;
            float dy = y_in_proj - (float)y1;

            // Zaciskanie współrzędnych do granic obrazu wejściowego
            x1 = clamp(x1, 0, input_width - 1);
            y1 = clamp(y1, 0, input_height - 1);
            x2 = clamp(x2, 0, input_width - 1);
            y2 = clamp(y2, 0, input_height - 1);

            // Wartości pikseli otaczających
            float p11 = input_band[pixel_index(x1, y1, input_width)];
            float p21 = input_band[pixel_index(x2, y1, input_width)];
            float p12 = input_band[pixel_index(x1, y2, input_width)];
            float p22 = input_band[pixel_index(x2, y2, input_width)];

            // Interpolacja dwuliniowa
            float interpolated_value =
                p11 * (1.0f - dx) * (1.0f - dy) +
                p21 * dx * (1.0f - dy) +
                p12 * (1.0f - dx) * dy +
                p22 * dx * dy;

            output_band[pixel_index(x_out, y_out, output_width)] = interpolated_value;
        }
    }
}

float* bilinear_resample_float(
    const float* input_band,
    int input_width,
    int input_height,
    int output_width,
    int output_height
)
{
    char* error_suffix = "in bilinear_resample_float.\n";

    float* output_band = prepare_data(
        input_band, input_width, input_height, output_width, output_height, error_suffix);

    if (output_band == NULL)
    {
        return NULL;
    }

    perform_bilinear_resample(input_band, output_band,
                              input_width, input_height,
                              output_width, output_height);

    return output_band;
}

void perform_average_resample(const float* input_band, float* output_band,
                              int input_width, int input_height,
                              int output_width, int output_height)
{
    // Współczynniki skalowania (ile pikseli wejściowych przypada na jeden piksel wyjściowy)
    float x_scale_factor = (float)input_width / output_width;
    float y_scale_factor = (float)input_height / output_height;

    for (int y_out = 0; y_out < output_height; y_out++)
    {
        for (int x_out = 0; x_out < output_width; x_out++)
        {
            // Początek bloku (lewy górny róg)
            int x_start_in = (int)roundf(x_out * x_scale_factor);
            int y_start_in = (int)roundf(y_out * y_scale_factor);
            // Koniec bloku (prawy dolny róg + 1, aby objąć piksele do uśrednienia)
            int x_end_in = (int)roundf((x_out + 1) * x_scale_factor);
            int y_end_in = (int)roundf((y_out + 1) * y_scale_factor);

            // Zaciskanie do granic obrazu wejściowego
            x_start_in = clamp(x_start_in, 0, input_width - 1);
            y_start_in = clamp(y_start_in, 0, input_height - 1);

            // Upewnij się, że jest co najmniej 1 piksel
            x_end_in = clamp(x_end_in, x_start_in + 1, input_width);
            y_end_in = clamp(y_end_in, y_start_in + 1, input_height);

            float sum = 0.0f;
            int count = 0;

            // Sumowanie wartości pikseli
            for (int y_in = y_start_in; y_in < y_end_in; y_in++)
            {
                for (int x_in = x_start_in; x_in < x_end_in; x_in++)
                {
                    sum += input_band[pixel_index(x_in, y_in, input_width)];
                    count++;
                }
            }

            // Obliczenie średniej lub fallback do najbliższego sąsiada
            if (count > 0)
            {
                output_band[pixel_index(x_out, y_out, output_width)] = sum / count;
            }
            else
            {
                // Sytuacja awaryjna, nie powinno się zdarzyć przy poprawnym zaciskaniu
                // Nie pozwalamy na brak przypisania żadnej wartości
                int center_x_in = (x_start_in + x_end_in - 1) / 2;
                int center_y_in = (y_start_in + y_end_in - 1) / 2;

                center_x_in = clamp(center_x_in, 0, input_width - 1);
                center_y_in = clamp(center_y_in, 0, input_height - 1);

                output_band[pixel_index(x_out, y_out, output_width)] =
                    input_band[pixel_index(center_x_in, center_y_in, input_width)];
            }
        }
    }
}

float* average_resample_float(
    const float* input_band,
    int input_width,
    int input_height,
    int output_width,
    int output_height
)
{
    char* error_suffix = "in average_resample_float.\n";

    float* output_band = prepare_data(
        input_band, input_width, input_height, output_width, output_height, error_suffix);

    if (output_band == NULL)
    {
        return NULL;
    }

    // Warning pomiędzy prepare_data a perform_average_resample
    if (output_width > input_width || output_height > input_height)
    {
        fprintf(stderr, "Warning: average_resample_float called for upsampling. Results may be incorrect.\n");
    }

    perform_average_resample(input_band, output_band,
                             input_width, input_height,
                             output_width, output_height);

    return output_band;
}

void replace_band_data(BandData* band_data, float* new_data)
{
    // Zwolnij dane
    if (*band_data->processed_data != *band_data->raw_data && *band_data->processed_data != NULL)
    {
        free(*band_data->processed_data);
    }

    // Zamień dane
    *band_data->processed_data = new_data;
}

int resample_single_band(BandData* band_data, int band_index, const ResamplingParams* params)
{
    // Sprawdź czy resampling jest potrzebny
    if (*band_data->width == params->target_width && *band_data->height == params->target_height)
    {
        return 0;
    }

    float* resampled = NULL;
    struct timeval start_time, end_time;
    double elapsed_time;

    if (params->is_10m_resolution)
    {
        // Upsampling do 10m rozdzielczości
        switch (band_index)
        {
        case B11:
            gettimeofday(&start_time, NULL);
            g_print("[%s] [B11] Rozpoczynam upsampling\n", get_timestamp());
            resampled = bilinear_resample_float(
                *band_data->raw_data,
                *band_data->width,
                *band_data->height,
                params->target_width,
                params->target_height
            );
            gettimeofday(&end_time, NULL);
            elapsed_time = get_time_diff(start_time, end_time);
            g_print("[%s] [B11] Zakończono upsampling (czas: %.2fs)\n", get_timestamp(), elapsed_time);
            break;

        case SCL:
            gettimeofday(&start_time, NULL);
            g_print("[%s] [SCL] Rozpoczynam upsampling\n", get_timestamp());
            resampled = nearest_neighbor_resample_scl(
                *band_data->raw_data,
                *band_data->width,
                *band_data->height,
                params->target_width,
                params->target_height
            );
            gettimeofday(&end_time, NULL);
            elapsed_time = get_time_diff(start_time, end_time);
            g_print("[%s] [SCL] Zakończono upsampling (czas: %.2fs)\n", get_timestamp(), elapsed_time);
            break;

        default:
            // B04 i B08 już mają rozdzielczość 10m
            return 0;
        }
    }
    else
    {
        // Downsampling do 20m rozdzielczości
        switch (band_index)
        {
        case B04:
        case B08:
            // B04 i B08 - averaging downsampling
            gettimeofday(&start_time, NULL);
            g_print("[%s] [%s] Rozpoczynam downsampling\n", get_timestamp(), band_data->band_name);
            resampled = average_resample_float(
                *band_data->raw_data,
                *band_data->width,
                *band_data->height,
                params->target_width,
                params->target_height
            );
            gettimeofday(&end_time, NULL);
            elapsed_time = get_time_diff(start_time, end_time);
            g_print("[%s] [%s] Zakończono downsampling (czas: %.2fs)\n", get_timestamp(), band_data->band_name, elapsed_time);
            break;

        default:
            // B11 i SCL już mają rozdzielczość 20m
            return 0;
        }
    }

    if (!resampled)
    {
        g_print("[%s] [%s] Błąd resamplingu.\n", get_timestamp(), band_data->band_name);
        return -1;
    }

    // Zamień dane na nowe po resamplingu
    replace_band_data(band_data, resampled);
    return 0;
}
