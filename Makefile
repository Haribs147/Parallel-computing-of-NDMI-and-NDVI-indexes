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

# Wszystkie flagi kompilatora
# -Wall włącza większość ostrzeżeń
# -g dodaje informacje debugowania
# -std=c11 dla standardu C11 (np. dla snprintf)
CFLAGS = $(GTK_CFLAGS) $(GDAL_CFLAGS) -Wall -g -std=c11

# Wszystkie biblioteki do linkowania
# -lm linkuje bibliotekę matematyczną (dla math.h)
LIBS = $(GTK_LIBS) $(GDAL_LIBS) -lm

# Pliki źródłowe
SRCS = main.c gui.c data_loader.c

# Pliki obiektowe (automatycznie generowane z .c na .o)
OBJS = $(SRCS:.c=.o)

# Domyślna reguła: buduje program
all: $(TARGET)

# Reguła linkowania: tworzy plik wykonywalny z plików obiektowych
$(TARGET): $(OBJS)
	@$(CC) $(OBJS) -o $(TARGET) $(LIBS)

# Reguły kompilacji: tworzy pliki obiektowe (.o) z plików źródłowych (.c)
main.o: main.c gui.h
	@$(CC) $(CFLAGS) -c main.c -o main.o

gui.o: gui.c gui.h data_loader.h 
	@$(CC) $(CFLAGS) -c gui.c -o gui.o

data_loader.o: data_loader.c data_loader.h
	@$(CC) $(CFLAGS) -c data_loader.c -o data_loader.o

# Reguła czyszczenia: usuwa plik wykonywalny i pliki obiektowe
clean:
	@rm -f $(TARGET) $(OBJS)

# Deklaracja, że 'all' i 'clean' nie są nazwami plików
.PHONY: all clean
