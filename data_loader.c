#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <gdal.h>
#include "data_loader.h"

// ====== UTILITY ======
const char* detect_band_from_filename(const char* filename);
char* get_timestamp();
double get_time_diff(struct timeval start, struct timeval end);
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

float* LoadBandData(const char* pszFilename, int* pnXSize, int* pnYSize) {
    const char* band_name = detect_band_from_filename(pszFilename);
    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);

    printf("[%s] [%s] Rozpoczynam wczytywanie pliku\n", get_timestamp(), band_name);

    // Walidacja wejścia
    if (!validate_filename(pszFilename)) {
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
    if (!validate_gdal_dataset(hDataset, pszFilename)) {
        return NULL;
    }

    // Pobieranie wymiarów
    nXSize = GDALGetRasterXSize(hDataset);
    nYSize = GDALGetRasterYSize(hDataset);

    // Walidacja wymiarów
    if (!validate_raster_dimensions(nXSize, nYSize, pszFilename)) {
        cleanup_gdal_resources(hDataset, NULL);
        return NULL;
    }

    // Ustawienie wymiarów wyjściowych
    set_output_dimensions(pnXSize, pnYSize, nXSize, nYSize);

    // Pobieranie pasma
    hBand = GDALGetRasterBand(hDataset, 1);
    if (!validate_raster_band(hBand, pszFilename)) {
        cleanup_gdal_resources(hDataset, NULL);
        return NULL;
    }

    // Alokacja bufora
    pafScanline = allocate_band_buffer(nXSize, nYSize, pszFilename);
    if (pafScanline == NULL) {
        cleanup_gdal_resources(hDataset, NULL);
        return NULL;
    }

    // Wczytywanie danych
    printf("[%s] [%s] Wczytywanie danych...\n", get_timestamp(), band_name);
    eErr = perform_raster_read(hBand, pafScanline, nXSize, nYSize);
    if (eErr != CE_None) {
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

const char* detect_band_from_filename(const char* filename) {
    if (filename == NULL) return "UNKNOWN";

    if (strstr(filename, "B04") || strstr(filename, "_B04_")) return "B04";
    if (strstr(filename, "B08") || strstr(filename, "_B08_")) return "B08";
    if (strstr(filename, "B11") || strstr(filename, "_B11_")) return "B11";
    if (strstr(filename, "SCL") || strstr(filename, "_SCL_")) return "SCL";

    return "UNKNOWN";
}

char* get_timestamp() {
    static char timestamp[64];
    struct timeval tv;
    struct tm* tm_info;

    gettimeofday(&tv, NULL);
    tm_info = localtime(&tv.tv_sec);

    snprintf(timestamp, sizeof(timestamp), "%02d:%02d:%02d.%03ld",
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec,
             tv.tv_usec / 1000);

    return timestamp;
}

double get_time_diff(struct timeval start, struct timeval end) {
    return (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
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
