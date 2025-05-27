# Kompilator C
CC = gcc
# Nazwa pliku wykonywalnego
TARGET = a.out
VALGRIND_TARGET = valgrind_test
# Opcje kompilatora i linkera dla GTK+ 3.0 (uzyskane z pkg-config)
GTK_CFLAGS = $(shell pkg-config --cflags gtk+-3.0)
GTK_LIBS = $(shell pkg-config --libs gtk+-3.0)
# Opcje kompilatora i linkera dla GDAL (uzyskane z gdal-config)
GDAL_CFLAGS = $(shell gdal-config --cflags)
GDAL_LIBS = $(shell gdal-config --libs)
# Flagi OpenMP
OMP_FLAGS = -fopenmp
# Wszystkie flagi kompilatora (przywrócono OMP_FLAGS)
CFLAGS = $(GTK_CFLAGS) $(GDAL_CFLAGS) $(OMP_FLAGS) -Wall -g -std=c11
# Wszystkie biblioteki do linkowania (przywrócono OMP_FLAGS)
LIBS = $(GTK_LIBS) $(GDAL_LIBS) $(OMP_FLAGS) -lm
# Pliki źródłowe
SRCS = main.c gui.c gui_utils.c data_loader.c resampler.c utils.c index_calculator.c visualization.c processing_pipeline.c data_saver.c
# Pliki obiektowe (automatycznie generowane z .c na .o)
OBJS = $(SRCS:.c=.o)
# Pliki obiektowe dla valgrind_test (bez gui.o i gui_utils.o, main.o)
VALGRIND_OBJS = data_loader.o resampler.o utils.o index_calculator.o visualization.o processing_pipeline.o

# Domyślna reguła: buduje program
all: $(TARGET)
# Reguła linkowania: tworzy plik wykonywalny z plików obiektowych
$(TARGET): $(OBJS)
	@$(CC) $(OBJS) -o $(TARGET) $(LIBS)

# Reguła dla valgrind_test: kompiluje i linkuje test
$(VALGRIND_TARGET): $(VALGRIND_OBJS) valgrind_test.o
	@$(CC) $(VALGRIND_OBJS) test/valgrind_test.o -o $(VALGRIND_TARGET) $(LIBS)

# Kompilacja valgrind_test.c (używamy tych samych flag co główny program)
valgrind_test.o: test/valgrind_test.c data_types.h data_loader.h resampler.h utils.h index_calculator.h visualization.h processing_pipeline.h
	@$(CC) $(CFLAGS) -c test/valgrind_test.c -o test/valgrind_test.o

# Reguły kompilacji
main.o: main.c gui.h
	@$(CC) $(CFLAGS) -c main.c -o main.o
gui.o: gui.c gui.h gui_utils.h data_loader.h resampler.h utils.h index_calculator.h visualization.h processing_pipeline.h data_types.h
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
processing_pipeline.o: processing_pipeline.c processing_pipeline.h data_loader.h resampler.h index_calculator.h utils.h data_types.h
	@$(CC) $(CFLAGS) -c processing_pipeline.c -o processing_pipeline.o
data_saver.o: data_saver.c data_saver.h
	@$(CC) $(CFLAGS) -c data_saver.c -o data_saver.o

# Reguła czyszczenia
clean:
	@rm -f $(TARGET) $(VALGRIND_TARGET) $(OBJS) test/valgrind_test.o test/valgrind_report.txt

# Reguła pomocnicza do uruchomienia valgrind
test-valgrind: $(VALGRIND_TARGET)
	@echo "Uruchamianie testu memory leak z małymi plikami..."
	@echo "Raport zostanie zapisany w test/valgrind_report.txt"
	@valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --log-file=test/valgrind_report.txt ./$(VALGRIND_TARGET) test/small_B04.jp2 test/small_B08.jp2 test/small_B11.jp2 test/small_SCL.jp2
	@echo "Test zakończony. Sprawdź test/valgrind_report.txt"

# Skrócona wersja - tylko buduje i uruchamia valgrind
valgrind: test-valgrind

.PHONY: all clean test-valgrind valgrind