# Kompilator C
CC = gcc

# Nazwa pliku wykonywalnego
TARGET = a.out

# Opcje kompilatora i linkera dla GTK+ 3.0 (uzyskane z pkg-config)
GTK_CFLAGS = $(shell pkg-config --cflags gtk+-3.0)
GTK_LIBS = $(shell pkg-config --libs gtk+-3.0)

# Opcje kompilatora i linkera dla GDAL (uzyskane z gdal-config)
GDAL_CFLAGS = $(shell gdal-config --cflags)
GDAL_LIBS = $(shell gdal-config --libs)

# Usunięto OMP_FLAGS
# Flagi OpenMP (opcjonalnie, jeśli chcesz użyć #pragma omp)
# OMP_FLAGS = -fopenmp

# Wszystkie flagi kompilatora
# Usunięto OMP_FLAGS z CFLAGS
CFLAGS = $(GTK_CFLAGS) $(GDAL_CFLAGS) -Wall -g -std=c11

# Wszystkie biblioteki do linkowania
# Usunięto OMP_FLAGS z LIBS
LIBS = $(GTK_LIBS) $(GDAL_LIBS) -lm

# Pliki źródłowe
SRCS = main.c gui.c gui_utils.c data_loader.c resampler.c utils.c index_calculator.c visualization.c

# Pliki obiektowe (automatycznie generowane z .c na .o)
OBJS = $(SRCS:.c=.o)

# Domyślna reguła: buduje program
all: $(TARGET)

# Reguła linkowania: tworzy plik wykonywalny z plików obiektowych
$(TARGET): $(OBJS)
	@$(CC) $(OBJS) -o $(TARGET) $(LIBS)
# Reguły kompilacji
main.o: main.c gui.h
	@$(CC) $(CFLAGS) -c main.c -o main.o

gui.o: gui.c gui.h gui_utils.h data_loader.h resampler.h utils.h index_calculator.h visualization.h
	@$(CC) $(CFLAGS) -c gui.c -o gui.o

gui_utils.o: gui_utils.c gui_utils.h utils.h
	@$(CC) $(CFLAGS) -c gui_utils.c -o gui_utils.o

data_loader.o: data_loader.c data_loader.h
	@$(CC) $(CFLAGS) -c data_loader.c -o data_loader.o

resampler.o: resampler.c resampler.h
	@$(CC) $(CFLAGS) -c resampler.c -o resampler.o

utils.o: utils.c utils.h
	@$(CC) $(CFLAGS) -c utils.c -o utils.o

index_calculator.o: index_calculator.c index_calculator.h
	@$(CC) $(CFLAGS) -c index_calculator.c -o index_calculator.o

visualization.o: visualization.c visualization.h index_calculator.h
	@$(CC) $(CFLAGS) -c visualization.c -o visualization.o

# Reguła czyszczenia
clean:
	@rm -f $(TARGET) $(OBJS)

.PHONY: all clean