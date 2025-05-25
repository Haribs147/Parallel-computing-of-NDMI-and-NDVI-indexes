#ifndef PROCESSING_PIPELINE_H
#define PROCESSING_PIPELINE_H

#include "data_types.h"
#include <stdbool.h>

typedef struct
{
    float* ndvi_data;
    float* ndmi_data;
    int width;
    int height;
} ProcessingResult;

/**
 * @brief Główna funkcja pipeline'u przetwarzania danych satelitarnych Sentinel-2
 *
 * Funkcja wykonuje kompletny proces przetwarzania danych satelitarnych:
 * 1. Waliduje dane wejściowe
 * 2. Wczytuje dane wszystkich pasm (B04, B08, B11, SCL)
 * 3. Wykonuje resampling do wspólnej rozdzielczości (10m lub 20m)
 * 4. Oblicza wskaźniki wegetacji NDVI i NDMI z zastosowaniem maski SCL
 * 5. Waliduje wyniki i zwraca strukturę ProcessingResult
 *
 * Pipeline automatycznie zarządza pamięcią - w przypadku błędu na którymkolwiek etapie
 * zwalnia już zaalokowane zasoby i zwraca NULL.
 *
 * @param bands Tablica 4 struktur BandData w określonej kolejności:
 *              - bands[0] (B04): Pasmo czerwone (RED), rozdzielczość 10m
 *              - bands[1] (B08): Pasmo bliskiej podczerwieni (NIR), rozdzielczość 10m
 *              - bands[2] (B11): Pasmo średniej podczerwieni (SWIR1), rozdzielczość 20m
 *              - bands[3] (SCL): Warstwa klasyfikacji sceny, rozdzielczość 20m
 *
 * @param target_10m Flaga określająca docelową rozdzielczość:
 *                   - true: upscaling do 10m (powiększenie pasm 20m)
 *                   - false: downscaling do 20m (zmniejszenie pasm 10m)
 *
 * @return Wskaźnik do struktury ProcessingResult zawierającej:
 *         - ndvi_data: Tablica wartości NDVI w zakresie [-1, 1]
 *         - ndmi_data: Tablica wartości NDMI w zakresie [-1, 1]
 *         - width, height: Wymiary wynikowych tablic w pikselach
 *
 *         NULL w przypadku błędu (szczegóły w stderr)
 *
 * @note Zwrócona struktura musi zostać zwolniona przez free_processing_result()
 * @note Funkcja automatycznie stosuje maskę SCL do wykluczenia nieprawidłowych pikseli
 * @note Po pomyślnym przetwarzaniu zwalnia pamięć danych pasm (bands)
 *
 * @warning Zakłada że tablica bands ma dokładnie 4 elementy w określonej kolejności
 * @warning Modyfikuje struktury BandData (zwalnia pamięć processed_data i raw_data)
 */
ProcessingResult* process_bands_and_calculate_indices(BandData bands[4], bool target_10m);

#endif // PROCESSING_PIPELINE_H
