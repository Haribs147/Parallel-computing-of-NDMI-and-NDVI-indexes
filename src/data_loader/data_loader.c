#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <gdal.h>
#include "data_loader.h"

#include <glib.h>

#include "../utils/utils.h"

// ====== WALIDACJA ======
int validate_filename(const char* filename);
int validate_gdal_dataset(GDALDatasetH dataset, const char* filename);
int validate_raster_dimensions(int width, int height, const char* filename);
int validate_raster_band(GDALRasterBandH band, const char* filename);
int validate_buffer_allocation(float* buffer, size_t num_pixels, const char* filename);
// ====== PAMIĘĆ ======
float* allocate_band_buffer(int width, int height, const char* filename);
void cleanup_gdal_resources(GDALDatasetH dataset, float* buffer);
// ====== FUNKCJONALNOŚĆ ======
CPLErr perform_raster_read(GDALRasterBandH band, float* buffer, int width, int height);
void set_output_dimensions(int* output_width, int* output_height, int width, int height);

int load_all_bands_data(BandData bands[4])
{
    int error_flag = 0;

    // Równoległe wczytywanie pasm
    #pragma omp parallel for shared(bands, error_flag)
    for (int i = 0; i < 4; i++)
    {
        // Wyjdź z pętli, jeżeli jedno z pasm napotkało błąd
        if (error_flag)
        {
            continue;
        }

        *(bands[i].raw_data) = LoadBandData(*(bands[i].path), bands[i].width, bands[i].height);

        if (!*(bands[i].raw_data))
        {
            #pragma omp critical
            {
                if (!error_flag)
                {
                    g_printerr("[%s] Błąd wczytywania pasma %s z pliku: %s\n",
                               get_timestamp(), bands[i].band_name, *(bands[i].path));
                    error_flag = 1;
                }
            }
        }
        else
        {
            // Ustawienie processed_data na raw_data (początkowo te same dane)
            *(bands[i].processed_data) = *(bands[i].raw_data);

            #pragma omp critical
            {
                g_print("[%s] Pomyślnie wczytano pasmo %s (%dx%d pikseli).\n",
                        get_timestamp(), bands[i].band_name, *bands[i].width, *bands[i].height);
            }
        }
    }

    // Jeśli wystąpił błąd, zwolnij już wczytane dane
    if (error_flag)
    {
        for (int i = 0; i < 4; i++)
        {
            if (*(bands[i].raw_data))
            {
                free(*(bands[i].raw_data));
                *(bands[i].raw_data) = NULL;
                *(bands[i].processed_data) = NULL;
            }
        }
        return -1;
    }

    return 0;
}

float* LoadBandData(const char* pszFilename, int* pnXSize, int* pnYSize)
{
    const char* band_name = detect_band_from_filename(pszFilename);
    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);

    printf("[%s] [%s] Rozpoczynam wczytywanie pliku\n", get_timestamp(), band_name);

    // Walidacja wejścia
    if (!validate_filename(pszFilename))
    {
        return NULL;
    }

    // Inicjalizacja zmiennych
    GDALDatasetH hDataset = NULL;
    GDALRasterBandH hBand = NULL;
    float* pafScanline = NULL;
    int nXSize = 0, nYSize = 0;
    CPLErr eErr;

    // Otwieranie datasetu
    hDataset = GDALOpen(pszFilename, GA_ReadOnly);
    if (!validate_gdal_dataset(hDataset, pszFilename))
    {
        return NULL;
    }

    // Pobieranie wymiarów
    nXSize = GDALGetRasterXSize(hDataset);
    nYSize = GDALGetRasterYSize(hDataset);

    // Walidacja wymiarów
    if (!validate_raster_dimensions(nXSize, nYSize, pszFilename))
    {
        cleanup_gdal_resources(hDataset, NULL);
        return NULL;
    }

    // Ustawienie wymiarów wyjściowych
    set_output_dimensions(pnXSize, pnYSize, nXSize, nYSize);

    // Pobieranie pasma
    hBand = GDALGetRasterBand(hDataset, 1);
    if (!validate_raster_band(hBand, pszFilename))
    {
        cleanup_gdal_resources(hDataset, NULL);
        return NULL;
    }

    // Alokacja bufora
    pafScanline = allocate_band_buffer(nXSize, nYSize, pszFilename);
    if (pafScanline == NULL)
    {
        cleanup_gdal_resources(hDataset, NULL);
        return NULL;
    }

    // Wczytywanie danych
    printf("[%s] [%s] Wczytywanie danych...\n", get_timestamp(), band_name);
    eErr = perform_raster_read(hBand, pafScanline, nXSize, nYSize);
    if (eErr != CE_None)
    {
        fprintf(stderr, "Błąd podczas wczytywania danych rastrowych z %s: %s\n",
                pszFilename, CPLGetLastErrorMsg());
        cleanup_gdal_resources(hDataset, pafScanline);
        return NULL;
    }

    // Cleanup (zachowujemy buffer, zamykamy dataset)
    GDALClose(hDataset);

    // Oblicz czas wykonania
    gettimeofday(&end_time, NULL);
    double elapsed_time = get_time_diff(start_time, end_time);

    printf("[%s] [%s] Zakończono (czas: %.2fs)\n",
           get_timestamp(), band_name, elapsed_time);

    return pafScanline;
}

int validate_filename(const char* filename)
{
    if (filename == NULL)
    {
        fprintf(stderr, "Error: Filename is NULL\n");
        return 0;
    }
    return 1;
}

int validate_gdal_dataset(GDALDatasetH dataset, const char* filename)
{
    if (dataset == NULL)
    {
        fprintf(stderr, "Nie można otworzyć pliku %s: %s\n", filename, CPLGetLastErrorMsg());
        return 0;
    }
    return 1;
}

int validate_raster_dimensions(int width, int height, const char* filename)
{
    if (width <= 0 || height <= 0)
    {
        fprintf(stderr, "Nieprawidłowe wymiary rastra w pliku %s (%dx%d).\n", filename, width, height);
        return 0;
    }
    return 1;
}

int validate_raster_band(GDALRasterBandH band, const char* filename)
{
    if (band == NULL)
    {
        fprintf(stderr, "Nie można pobrać pasma z pliku %s: %s\n", filename, CPLGetLastErrorMsg());
        return 0;
    }
    return 1;
}

int validate_buffer_allocation(float* buffer, size_t num_pixels, const char* filename)
{
    if (buffer == NULL)
    {
        fprintf(stderr, "Błąd alokacji pamięci dla %zu pikseli danych pasma z pliku %s.\n", num_pixels, filename);
        return 0;
    }
    return 1;
}

float* allocate_band_buffer(int width, int height, const char* filename)
{
    size_t num_pixels = (size_t)width * height;

    float* buffer = malloc(sizeof(float) * num_pixels);

    if (!validate_buffer_allocation(buffer, num_pixels, filename))
    {
        return NULL;
    }

    return buffer;
}

void cleanup_gdal_resources(GDALDatasetH dataset, float* buffer)
{
    if (buffer != NULL)
    {
        free(buffer);
    }
    if (dataset != NULL)
    {
        GDALClose(dataset);
    }
}

CPLErr perform_raster_read(GDALRasterBandH band, float* buffer, int width, int height)
{
    return GDALRasterIO(band, GF_Read, 0, 0, width, height,
                        buffer, width, height, GDT_Float32,
                        0, 0);
}

void set_output_dimensions(int* output_width, int* output_height, int width, int height)
{
    if (output_width != NULL)
    {
        *output_width = width;
    }
    if (output_height != NULL)
    {
        *output_height = height;
    }
}
