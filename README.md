# System Analizy Indeksów Roślinności Sentinel-2

Program do równoległego obliczania wskaźników wilgotności (NDMI) i wegetacji (NDVI) roślin na zobrazowaniach satelitarnych Sentinel-2, napisany w języku C z interfejsem graficznym GTK.

## Opis Projektu

Aplikacja umożliwia:
- Wczytywanie i przetwarzanie pasm satelitarnych Sentinel-2 (B04, B08, B11, SCL)
- Równoległe obliczanie wskaźników NDVI i NDMI
- Automatyczny resampling danych do wspólnej rozdzielczości (10m lub 20m)
- Maskowanie niepożądanych pikseli (chmury, woda, śnieg) przy użyciu warstwy SCL
- Wizualizację wyników w interfejsie graficznym
- Eksport map wskaźników do plików PNG

## Wymagania Systemowe

### Zależności
- **Kompilator:** GCC z obsługą C11 i OpenMP
- **Biblioteki:**
    - GTK+ 3.0
    - GDAL (Geospatial Data Abstraction Library)
    - OpenMP
    - pkg-config

### Instalacja zależności (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install build-essential gcc
sudo apt install libgtk-3-dev
sudo apt install libgdal-dev gdal-bin
sudo apt install libomp-dev
sudo apt install pkg-config
```

### Instalacja zależności (Arch Linux)
```bash
sudo pacman -S base-devel gcc
sudo pacman -S gtk3
sudo pacman -S gdal
sudo pacman -S openmp
sudo pacman -S pkg-config
```

## Kompilacja i Uruchomienie

```bash
# Klonowanie repozytorium
git clone git@github.com:Haribs147/Parallel-computing-of-NDMI-and-NDVI-indexes.git
cd Parallel-computing-of-NDMI-and-NDVI-indexes.git

# Kompilacja
make

# Uruchomienie
./program.out
```

### Czyszczenie plików kompilacji
```bash
make clean
```

## Dane Wejściowe

Program operuje na 4 plikach `.jp2` z satelity Sentinel-2:

1. **B04** - Pasmo czerwone (RED), rozdzielczość 10m
2. **B08** - Pasmo bliskiej podczerwieni (NIR), rozdzielczość 10m
3. **B11** - Pasmo średniej podczerwieni (SWIR1), rozdzielczość 20m
4. **SCL** - Warstwa klasyfikacji sceny, rozdzielczość 20m

### Pozyskiwanie danych
Dane można pobrać z [Copernicus Browser](https://browser.dataspace.copernicus.eu/) w formatach L1C lub L2A.

## Obliczane wskaźniki

### NDVI (Normalized Difference Vegetation Index)
```
NDVI = (NIR - RED) / (NIR + RED)
```
- **Zakres:** [-1, 1]
- **Interpretacja:** Miara kondycji i gęstości roślinności

### NDMI (Normalized Difference Moisture Index)
```
NDMI = (NIR - SWIR1) / (NIR + SWIR1)
```
- **Zakres:** [-1, 1]
- **Interpretacja:** Miara zawartości wody w roślinności

## Architektura Programu

- **`data_loader`** - Wczytywanie plików .jp2 przy użyciu GDAL
- **`resampler`** - Algorytmy resamplingu z obsługą OpenMP
- **`index_calculator`** - Obliczanie NDVI i NDMI z maskowaniem SCL
- **`visualization`** - Mapowanie wartości na kolory RGB
- **`gui`** - Interfejs użytkownika GTK
- **`processing_pipeline`** - Orkiestracja całego procesu
- **`utils`** - Funkcje pomocnicze

## Dokumentacja Techniczna

Pełna dokumentacja techniczna dostępna w folderze `/doc`.