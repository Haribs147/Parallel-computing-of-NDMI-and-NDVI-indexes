#ifndef DATA_LOADER_H
#define DATA_LOADER_H
#include "../data_types/data_types.h"

/**
 * @brief Wczytuje dane pasma satelitarnego z pliku GDAL-kompatybilnego
 *
 * Funkcja otwiera plik rasterowy przy użyciu biblioteki GDAL, waliduje jego poprawność,
 * alokuje pamięć i wczytuje dane pierwszego pasma jako tablicę wartości float.
 *
 * @param pszFilename Ścieżka do pliku rasterowego (np. plik .jp2 z danymi Sentinel-2)
 * @param pnXSize Wskaźnik do zmiennej, w której zostanie zapisana szerokość rastra w pikselach
 * @param pnYSize Wskaźnik do zmiennej, w której zostanie zapisana wysokość rastra w pikselach
 *
 * @return Wskaźnik do zaalokowanej tablicy float zawierającej dane pikseli pasma,
 *         lub NULL w przypadku błędu (nieprawidłowy plik, błąd alokacji pamięci itp.)
 *
 * @note Zwrócona pamięć musi zostać zwolniona przez wywołującego przy użyciu free()
 * @note Funkcja automatycznie wykrywa typ pasma na podstawie nazwy pliku i loguje postęp
 */
float* LoadBandData(const char* pszFilename, int* pnXSize, int* pnYSize);
/**
 * @brief Wczytuje dane dla wszystkich czterech pasm satelitarnych Sentinel-2
 *
 * Funkcja iteruje przez tablicę struktur BandData i wczytuje dane dla każdego pasma
 * przy użyciu funkcji LoadBandData(). W przypadku błędu dla któregokolwiek pasma,
 * automatycznie zwalnia już wczytane dane i zwraca błąd.
 *
 * @param bands Tablica 4 struktur BandData zawierających informacje o pasmach do wczytania.
 *              Każda struktura musi zawierać:
 *              - Wskaźnik do ścieżki pliku (path)
 *              - Wskaźniki do zmiennych wymiarów (width, height)
 *              - Wskaźniki do buforów danych (raw_data, processed_data)
 *              - Nazwę pasma (band_name) do logowania
 *
 * @return 0 w przypadku sukcesu (wszystkie pasma wczytane pomyślnie),
 *         - -1 w przypadku błędu (brak ścieżki, błąd wczytywania lub alokacji pamięci)
 *
 * @warning Zakłada, że tablica bands ma dokładnie 4 elementy
 */
int load_all_bands_data(BandData bands[4]);

#endif
