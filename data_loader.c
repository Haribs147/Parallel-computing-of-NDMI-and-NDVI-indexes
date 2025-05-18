#include <stdio.h>
#include <stdlib.h>
#include <string.h> // Dla CPLGetLastErrorMsg w niektórych konfiguracjach GDAL
#include <gdal.h>

#include "data_loader.h"

// Definicja funkcji LoadBandData (przeniesiona z Twojego "kodzika")
float* LoadBandData(const char* pszFilename, int* pnXSize, int* pnYSize) {
    GDALDatasetH hDataset;
    GDALRasterBandH hBand;
    float* pafScanline = NULL; // Zainicjuj na NULL
    int nXSize = 0, nYSize = 0; // Zainicjuj
    CPLErr eErr;

    // GDALAllRegister() jest teraz wywoływane w gui.c w run_gui(), więc nie tutaj.

    hDataset = GDALOpen(pszFilename, GA_ReadOnly);
    if (hDataset == NULL) {
        fprintf(stderr, "Nie można otworzyć pliku %s: %s\n", pszFilename, CPLGetLastErrorMsg());
        return NULL;
    }

    nXSize = GDALGetRasterXSize(hDataset);
    nYSize = GDALGetRasterYSize(hDataset);
    
    // Zapisz wymiary tylko jeśli wskaźniki nie są NULL
    if (pnXSize != NULL) {
        *pnXSize = nXSize;
    }
    if (pnYSize != NULL) {
        *pnYSize = nYSize;
    }

    // Sprawdź, czy wymiary są poprawne przed alokacją pamięci
    if (nXSize <= 0 || nYSize <= 0) {
        fprintf(stderr, "Nieprawidłowe wymiary rastra w pliku %s (%dx%d).\n", pszFilename, nXSize, nYSize);
        GDALClose(hDataset);
        return NULL;
    }

    hBand = GDALGetRasterBand(hDataset, 1);
    if (hBand == NULL) {
        fprintf(stderr, "Nie można pobrać pasma z pliku %s: %s\n", pszFilename, CPLGetLastErrorMsg());
        GDALClose(hDataset);
        return NULL;
    }

    // Użyj size_t dla iloczynu, aby uniknąć przepełnienia przy dużych obrazach
    size_t num_pixels = (size_t)nXSize * nYSize;
    pafScanline = (float*)malloc(sizeof(float) * num_pixels);
    if (pafScanline == NULL) {
        fprintf(stderr, "Błąd alokacji pamięci dla %zu pikseli danych pasma z pliku %s.\n", num_pixels, pszFilename);
        GDALClose(hDataset);
        return NULL;
    }

    eErr = GDALRasterIO(hBand, GF_Read, 0, 0, nXSize, nYSize,
                        pafScanline, nXSize, nYSize, GDT_Float32,
                        0, 0);

    if (eErr != CE_None) {
        fprintf(stderr, "Błąd podczas wczytywania danych rastrowych z %s: %s\n", pszFilename, CPLGetLastErrorMsg());
        free(pafScanline); // Zwolnij pamięć w przypadku błędu IO
        GDALClose(hDataset);
        return NULL;
    }

    GDALClose(hDataset);
    return pafScanline;
}
