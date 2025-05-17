#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gdal.h>

/**
 * @brief Wczytuje dane rastrowe z pliku JP2 do tablicy float.
 *
 * @param pszFilename Ścieżka do pliku rastrowego.
 * @param pnXSize Wskaźnik do zmiennej, gdzie zostanie zapisana szerokość rastra.
 * @param pnYSize Wskaźnik do zmiennej, gdzie zostanie zapisana wysokość rastra.
 * @return Wskaźnik do zaalokowanej tablicy float z danymi pikseli,
 * lub NULL w przypadku błędu.
 */ 
float* LoadBandData(const char* pszFilename, int* pnXSize, int* pnYSize) {
    GDALDatasetH hDataset;
    GDALRasterBandH hBand;
    float* pafScanline;
    int nXSize, nYSize;
    CPLErr eErr;

    // Otwarcie zbioru danych (pliku rastrowego)
    hDataset = GDALOpen(pszFilename, GA_ReadOnly);
    if (hDataset == NULL) {
        fprintf(stderr, "Nie można otworzyć pliku %s: %s\n", pszFilename, CPLGetLastErrorMsg());
        return NULL;
    }

    // Pobranie wymiarów rastra
    nXSize = GDALGetRasterXSize(hDataset);
    nYSize = GDALGetRasterYSize(hDataset);
    *pnXSize = nXSize;
    *pnYSize = nYSize;

    // Pobranie pierwszego pasma (zakładamy, że pliki .jp2 dla pasm są jednopasmowe)
    hBand = GDALGetRasterBand(hDataset, 1);
    if (hBand == NULL) {
        fprintf(stderr, "Nie można pobrać pasma z pliku %s: %s\n", pszFilename, CPLGetLastErrorMsg());
        GDALClose(hDataset);
        return NULL;
    }

    // Alokacja pamięci na dane pikseli
    // Wczytujemy całe pasmo do jednej tablicy
    pafScanline = (float*)malloc(sizeof(float) * nXSize * nYSize);
    if (pafScanline == NULL) {
        fprintf(stderr, "Błąd alokacji pamięci dla danych pasma z pliku %s.\n", pszFilename);
        GDALClose(hDataset);
        return NULL;
    }

    // Wczytanie danych rastrowych do tablicy
    // Wczytujemy cały obraz (od 0,0 do nXSize,nYSize)
    // do bufora pafScanline, konwertując dane do typu GDT_Float32.
    eErr = GDALRasterIO(hBand, GF_Read, 0, 0, nXSize, nYSize,
                        pafScanline, nXSize, nYSize, GDT_Float32,
                        0, 0);

    if (eErr != CE_None) {
        fprintf(stderr, "Błąd podczas wczytywania danych rastrowych z %s: %s\n", pszFilename, CPLGetLastErrorMsg());
        free(pafScanline);
        GDALClose(hDataset);
        return NULL;
    }

    // Zamknięcie zbioru danych
    GDALClose(hDataset);
    return pafScanline;
}

int main() {
    GDALAllRegister();

    const char* pszBand4File  = "T34UFD_20250513T094051_B04_10m.jp2";
    const char* pszBand8File  = "T34UFD_20250513T094051_B08_10m.jp2";
    const char* pszBand11File = "T34UFD_20250513T094051_B11_20m.jp2";
    const char* pszSCLFile    = "T34UFD_20250513T094051_SCL_20m.jp2";


    float* pafBand4Data  = NULL;
    float* pafBand8Data  = NULL;
    float* pafBand11Data = NULL;
    float* pafSCLData    = NULL;

    int nBand4Width, nBand4Height;
    int nBand8Width, nBand8Height;
    int nBand11Width, nBand11Height;
    int nSCLWidth, nSCLHeight;

    printf("Rozpoczynanie wczytywania pasm...\n");

    pafBand4Data = LoadBandData(pszBand4File, &nBand4Width, &nBand4Height);
    pafBand8Data = LoadBandData(pszBand8File, &nBand8Width, &nBand8Height);
    pafBand11Data = LoadBandData(pszBand11File, &nBand11Width, &nBand11Height);
    pafSCLData = LoadBandData(pszSCLFile, &nSCLWidth, &nSCLHeight);

    
    if (!pafBand4Data || !pafBand8Data || !pafBand11Data || !pafSCLData) {
        fprintf(stderr, "\n>>> Nie udało się wczytać wszystkich niezbędnych pasm. Pomijam dalsze przetwarzanie.\n");
        free(pafBand4Data); // TODO zrobić GOTO CLEANUP albo FUNKCJĘ CLEANUP
        free(pafBand8Data);
        free(pafBand11Data);
        free(pafSCLData);
        return 1;
    }
    
    printf("Zwalnianie pamięci...\n");
    free(pafBand4Data);
    free(pafBand8Data);
    free(pafBand11Data);
    free(pafSCLData);
    
    printf("Zakończono program.\n");
    return 0;
}