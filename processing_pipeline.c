#include "processing_pipeline.h"
#include "data_loader.h"
#include "resampler.h"
#include "index_calculator.h"
#include "utils.h"
#include "data_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ====== GŁÓWNA FUNKCJA ======
ProcessingResult* process_bands_and_calculate_indices(BandData bands[4], bool target_10m);

// ====== FUNKCJE POMOCNICZE ======
static void get_target_resolution_dimensions(const BandData* bands, bool target_10m,
                                     int* width_out, int* height_out);

// ====== PAMIĘĆ ======
static void free_processing_result(ProcessingResult* result);
static void free_band_data(BandData bands[4]);

// ====== WALIDACJA ======
static int validate_processing_inputs(const BandData bands[4]);
static int validate_processing_result(const ProcessingResult* result);

ProcessingResult* process_bands_and_calculate_indices(BandData bands[4], bool target_10m)
{
    if (!validate_processing_inputs(bands))
    {
        fprintf(stderr, "[%s] Błąd walidacji danych wejściowych.\n", get_timestamp());
        return NULL;
    }

    ProcessingResult* result = malloc(sizeof(ProcessingResult));
    if (!result)
    {
        fprintf(stderr, "[%s] Błąd alokacji pamięci dla ProcessingResult.\n", get_timestamp());
        return NULL;
    }

    // Inicjalizacja wyniku
    result->ndvi_data = NULL;
    result->ndmi_data = NULL;
    result->width = 0;
    result->height = 0;

    // Ładowanie danych pasm
    if (load_all_bands_data(bands) != 0)
    {
        fprintf(stderr, "[%s] Błąd ładowania danych pasm.\n", get_timestamp());
        free(result);
        return NULL;
    }

    // Określenie docelowych wymiarów
    get_target_resolution_dimensions(bands, target_10m, &result->width, &result->height);

    // Resampling pasm do docelowej rozdzielczości
    if (resample_all_bands_to_target_resolution(bands, 4, target_10m) != 0)
    {
        fprintf(stderr, "[%s] Błąd resamplingu pasm.\n", get_timestamp());
        free_band_data(bands);
        free(result);
        return NULL;
    }

    // Obliczanie NDVI
    result->ndvi_data = calculate_ndvi(*bands[B08].processed_data, *bands[B04].processed_data,
                                       result->width, result->height,
                                       *bands[SCL].processed_data);
    if (!result->ndvi_data)
    {
        fprintf(stderr, "[%s] Błąd podczas obliczania NDVI.\n", get_timestamp());
        free_band_data(bands);
        free(result);
        return NULL;
    }

    // Obliczanie NDMI
    result->ndmi_data = calculate_ndmi(*bands[B08].processed_data, *bands[B11].processed_data,
                                       result->width, result->height,
                                       *bands[SCL].processed_data);
    if (!result->ndmi_data)
    {
        fprintf(stderr, "[%s] Błąd podczas obliczania NDMI.\n", get_timestamp());
        free(result->ndvi_data);
        free_band_data(bands);
        free(result);
        return NULL;
    }

    // Zwalnianie danych pasm - już nie potrzebne
    free_band_data(bands);

    if (!validate_processing_result(result))
    {
        fprintf(stderr, "[%s] Błąd walidacji wyników przetwarzania.\n", get_timestamp());
        free_processing_result(result);
        return NULL;
    }

    printf("[%s] Przetwarzanie zakończone pomyślnie. Wymiary: %dx%d\n",
           get_timestamp(), result->width, result->height);

    return result;
}


static void get_target_resolution_dimensions(const BandData* bands, bool target_10m,
                                     int* width_out, int* height_out)
{
    if (target_10m)
    {
        // Używamy wymiarów pasm 10m (B04, B08)
        *width_out = *bands[B04].width;
        *height_out = *bands[B04].height;
    }
    else
    {
        // Używamy wymiarów pasm 20m (B11, SCL)
        *width_out = *bands[B11].width;
        *height_out = *bands[B11].height;
    }

    printf("[%s] Docelowa rozdzielczość: %dm, wymiary: %dx%d\n",
           get_timestamp(), target_10m ? 10 : 20, *width_out, *height_out);
}

static void free_processing_result(ProcessingResult* result)
{
    if (!result)
    {
        return;
    }

    if (result->ndvi_data)
    {
        free(result->ndvi_data);
        result->ndvi_data = NULL;
    }

    if (result->ndmi_data)
    {
        free(result->ndmi_data);
        result->ndmi_data = NULL;
    }

    free(result);
    printf("[%s] Zwolniono pamięć ProcessingResult.\n", get_timestamp());
}

static void free_band_data(BandData bands[4])
{
    for (int i = 0; i < 4; i++)
    {
        // Zwolnienie processed_data jeśli różni się od raw_data
        if (*(bands[i].processed_data) && *(bands[i].processed_data) != *(bands[i].raw_data))
        {
            free(*(bands[i].processed_data));
            *(bands[i].processed_data) = NULL;
        }

        // Zwolnienie raw_data
        if (*(bands[i].raw_data))
        {
            free(*(bands[i].raw_data));
            *(bands[i].raw_data) = NULL;
        }

        *(bands[i].processed_data) = NULL;
    }

    printf("[%s] Zwolniono pamięć danych pasm.\n", get_timestamp());
}

static int validate_processing_inputs(const BandData bands[4])
{
    for (int i = 0; i < 4; i++)
    {
        if (!bands[i].path)
        {
            fprintf(stderr, "[%s] Błąd walidacji: Brak ścieżki dla pasma %s.\n",
                    get_timestamp(), bands[i].band_name);
            return 0;
        }

        if (!bands[i].width || !bands[i].height)
        {
            fprintf(stderr, "[%s] Błąd walidacji: Brak wskaźników wymiarów dla pasma %s.\n",
                    get_timestamp(), bands[i].band_name);
            return 0;
        }

        if (!bands[i].raw_data || !bands[i].processed_data)
        {
            fprintf(stderr, "[%s] Błąd walidacji: Brak wskaźników danych dla pasma %s.\n",
                    get_timestamp(), bands[i].band_name);
            return 0;
        }
    }

    return 1;
}

static int validate_processing_result(const ProcessingResult* result)
{
    if (!result)
    {
        fprintf(stderr, "[%s] Błąd walidacji: ProcessingResult jest NULL.\n", get_timestamp());
        return 0;
    }

    if (!result->ndvi_data)
    {
        fprintf(stderr, "[%s] Błąd walidacji: Brak danych NDVI w wyniku.\n", get_timestamp());
        return 0;
    }

    if (!result->ndmi_data)
    {
        fprintf(stderr, "[%s] Błąd walidacji: Brak danych NDMI w wyniku.\n", get_timestamp());
        return 0;
    }

    if (result->width <= 0 || result->height <= 0)
    {
        fprintf(stderr, "[%s] Błąd walidacji: Nieprawidłowe wymiary wyniku: %dx%d.\n",
                get_timestamp(), result->width, result->height);
        return 0;
    }

    return 1;
}