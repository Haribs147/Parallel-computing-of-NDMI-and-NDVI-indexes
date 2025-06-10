# Kompilator C
CC = gcc
# Nazwa pliku wykonywalnego
TARGET = program.out
# Folder dla plików obiektowych
OUTPUT_DIR = output
# Opcje kompilatora i linkera dla GTK+ 3.0
GTK_CFLAGS = $(shell pkg-config --cflags gtk+-3.0)
GTK_LIBS = $(shell pkg-config --libs gtk+-3.0)
# Opcje kompilatora i linkera dla GDAL
GDAL_CFLAGS = $(shell gdal-config --cflags)
GDAL_LIBS = $(shell gdal-config --libs)
# Flagi OpenMP
OMP_FLAGS = -fopenmp
# Wszystkie flagi kompilatora
CFLAGS = $(GTK_CFLAGS) $(GDAL_CFLAGS) $(OMP_FLAGS) -Wall -g -std=c11
# Wszystkie biblioteki do linkowania (przywrócono OMP_FLAGS)
LIBS = $(GTK_LIBS) $(GDAL_LIBS) $(OMP_FLAGS) -lm
# Pliki źródłowe
SRCS = src/main.c src/gui/gui.c src/utils/gui_utils.c src/data_loader/data_loader.c src/resampler/resampler.c src/utils/utils.c src/index_calculator/index_calculator.c src/visualization/visualization.c src/processing_pipeline/processing_pipeline.c src/data_saver/data_saver.c
# Pliki obiektowe (output)
OBJS = $(SRCS:src/%.c=$(OUTPUT_DIR)/%.o)
# Domyślna reguła: buduje program
all: $(OUTPUT_DIR) $(TARGET)
# Tworzy folder output jeśli nie istnieje
$(OUTPUT_DIR):
	@mkdir -p $(OUTPUT_DIR)
# Reguła linkowania: tworzy plik wykonywalny z plików obiektowych
$(TARGET): $(OBJS)
	@$(CC) $(OBJS) -o $(TARGET) $(LIBS)
# Reguły kompilacji
$(OUTPUT_DIR)/main.o: src/main.c src/gui/gui.h | $(OUTPUT_DIR)
	@$(CC) $(CFLAGS) -c src/main.c -o $(OUTPUT_DIR)/main.o
$(OUTPUT_DIR)/gui/gui.o: src/gui/gui.c src/gui/gui.h src/utils/gui_utils.h src/data_loader/data_loader.h src/resampler/resampler.h src/utils/utils.h src/index_calculator/index_calculator.h src/visualization/visualization.h src/processing_pipeline/processing_pipeline.h src/data_types/data_types.h | $(OUTPUT_DIR)
	@mkdir -p $(OUTPUT_DIR)/gui
	@$(CC) $(CFLAGS) -c src/gui/gui.c -o $(OUTPUT_DIR)/gui/gui.o
$(OUTPUT_DIR)/utils/gui_utils.o: src/utils/gui_utils.c src/utils/gui_utils.h src/utils/utils.h | $(OUTPUT_DIR)
	@mkdir -p $(OUTPUT_DIR)/utils
	@$(CC) $(CFLAGS) -c src/utils/gui_utils.c -o $(OUTPUT_DIR)/utils/gui_utils.o
$(OUTPUT_DIR)/data_loader/data_loader.o: src/data_loader/data_loader.c src/data_loader/data_loader.h | $(OUTPUT_DIR)
	@mkdir -p $(OUTPUT_DIR)/data_loader
	@$(CC) $(CFLAGS) -c src/data_loader/data_loader.c -o $(OUTPUT_DIR)/data_loader/data_loader.o
$(OUTPUT_DIR)/resampler/resampler.o: src/resampler/resampler.c src/resampler/resampler.h | $(OUTPUT_DIR)
	@mkdir -p $(OUTPUT_DIR)/resampler
	@$(CC) $(CFLAGS) -c src/resampler/resampler.c -o $(OUTPUT_DIR)/resampler/resampler.o
$(OUTPUT_DIR)/utils/utils.o: src/utils/utils.c src/utils/utils.h | $(OUTPUT_DIR)
	@mkdir -p $(OUTPUT_DIR)/utils
	@$(CC) $(CFLAGS) -c src/utils/utils.c -o $(OUTPUT_DIR)/utils/utils.o
$(OUTPUT_DIR)/index_calculator/index_calculator.o: src/index_calculator/index_calculator.c src/index_calculator/index_calculator.h | $(OUTPUT_DIR)
	@mkdir -p $(OUTPUT_DIR)/index_calculator
	@$(CC) $(CFLAGS) -c src/index_calculator/index_calculator.c -o $(OUTPUT_DIR)/index_calculator/index_calculator.o
$(OUTPUT_DIR)/visualization/visualization.o: src/visualization/visualization.c src/visualization/visualization.h src/index_calculator/index_calculator.h | $(OUTPUT_DIR)
	@mkdir -p $(OUTPUT_DIR)/visualization
	@$(CC) $(CFLAGS) -c src/visualization/visualization.c -o $(OUTPUT_DIR)/visualization/visualization.o
$(OUTPUT_DIR)/processing_pipeline/processing_pipeline.o: src/processing_pipeline/processing_pipeline.c src/processing_pipeline/processing_pipeline.h src/data_loader/data_loader.h src/resampler/resampler.h src/index_calculator/index_calculator.h src/utils/utils.h src/data_types/data_types.h | $(OUTPUT_DIR)
	@mkdir -p $(OUTPUT_DIR)/processing_pipeline
	@$(CC) $(CFLAGS) -c src/processing_pipeline/processing_pipeline.c -o $(OUTPUT_DIR)/processing_pipeline/processing_pipeline.o
$(OUTPUT_DIR)/data_saver/data_saver.o: src/data_saver/data_saver.c src/data_saver/data_saver.h | $(OUTPUT_DIR)
	@mkdir -p $(OUTPUT_DIR)/data_saver
	@$(CC) $(CFLAGS) -c src/data_saver/data_saver.c -o $(OUTPUT_DIR)/data_saver/data_saver.o
# Reguła czyszczenia
clean:
	@rm -f $(TARGET)
	@rm -rf $(OUTPUT_DIR)
.PHONY: all clean