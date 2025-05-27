#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gdal.h>
#include "../data_types.h"
#include "../data_loader.h"
#include "../resampler.h"
#include "../utils.h"
#include "../index_calculator.h"
#include "../processing_pipeline.h"
#include "../visualization.h"

// Structure to hold pixbuf data for testing
typedef struct
{
    ProcessingResult* map_data;
    GdkPixbuf* ndvi_pixbuf; // Real GdkPixbuf pointers
    GdkPixbuf* ndmi_pixbuf;
} TestMapWindowData;

// Extracted business logic from on_rozpocznij_clicked
static ProcessingResult* extract_processing_logic(char* path_b04, char* path_b08,
                                                  char* path_b11, char* path_scl,
                                                  int res_10m_selected)
{
    // Prepare band data structures
    int widths[4], heights[4];
    float* raw_data[4] = {NULL};
    float* processed_data[4] = {NULL};

    BandData bands[4] = {
        {&path_b04, &raw_data[0], &processed_data[0], &widths[0], &heights[0], "B04"},
        {&path_b08, &raw_data[1], &processed_data[1], &widths[1], &heights[1], "B08"},
        {&path_b11, &raw_data[2], &processed_data[2], &widths[2], &heights[2], "B11"},
        {&path_scl, &raw_data[3], &processed_data[3], &widths[3], &heights[3], "SCL"}
    };

    // Validate band paths
    for (int i = 0; i < 4; i++)
    {
        if (!*(bands[i].path))
        {
            printf("Error: Missing file path for band %s\n", bands[i].band_name);
            return NULL;
        }
        printf("Using file: %s for band %s\n", *(bands[i].path), bands[i].band_name);
    }

    printf("[%s] Starting band data processing.\n", get_timestamp());

    ProcessingResult* processing_result = process_bands_and_calculate_indices(bands, res_10m_selected);

    if (!processing_result)
    {
        printf("[%s] Error occurred during data processing.\n", get_timestamp());
        return NULL;
    }

    printf("[%s] Processing completed successfully.\n", get_timestamp());
    return processing_result;
}

// Extracted cleanup logic from map_window_data_destroy
static void extract_cleanup_logic(ProcessingResult* map_data)
{
    printf("[%s] Starting cleanup process.\n", get_timestamp());

    if (!map_data)
    {
        printf("[%s] Map data is NULL, skipping cleanup.\n", get_timestamp());
        return;
    }

    GdkPixbuf* ndvi_pixbuf = NULL;
    GdkPixbuf* ndmi_pixbuf = NULL;

    if (map_data->ndvi_data)
    {
        printf("[%s] Generating NDVI pixbuf using real visualization logic...\n", get_timestamp());
        ndvi_pixbuf = generate_pixbuf_from_index_data(map_data->ndvi_data,
                                                      map_data->width, map_data->height);
        printf("[%s] NDVI pixbuf created: %p\n", get_timestamp(), (void*)ndvi_pixbuf);
    }

    if (map_data->ndmi_data)
    {
        printf("[%s] Generating NDMI pixbuf using real visualization logic...\n", get_timestamp());
        ndmi_pixbuf = generate_pixbuf_from_index_data(map_data->ndmi_data,
                                                      map_data->width, map_data->height);
        printf("[%s] NDMI pixbuf created: %p\n", get_timestamp(), (void*)ndmi_pixbuf);
    }

    if (ndvi_pixbuf)
    {
        printf("[%s] Freeing NDVI pixbuf: %p\n", get_timestamp(), (void*)ndvi_pixbuf);
        g_object_unref(ndvi_pixbuf);
        ndvi_pixbuf = NULL;
    }
    if (ndmi_pixbuf)
    {
        printf("[%s] Freeing NDMI pixbuf: %p\n", get_timestamp(), (void*)ndmi_pixbuf);
        g_object_unref(ndmi_pixbuf);
        ndmi_pixbuf = NULL;
    }

    // Cleanup map data (extracted from free_index_map_data)
    printf("[%s] Starting map data cleanup.\n", get_timestamp());

    if (map_data->ndvi_data)
    {
        printf("[%s] Freeing NDVI data: %p\n", get_timestamp(), (void*)map_data->ndvi_data);
        free(map_data->ndvi_data);
        map_data->ndvi_data = NULL;
    }

    if (map_data->ndmi_data)
    {
        printf("[%s] Freeing NDMI data: %p\n", get_timestamp(), (void*)map_data->ndmi_data);
        free(map_data->ndmi_data);
        map_data->ndmi_data = NULL;
    }

    printf("[%s] Freeing ProcessingResult structure: %p\n", get_timestamp(), (void*)map_data);
    free(map_data);

    printf("[%s] Cleanup completed.\n", get_timestamp());
}

int main(int argc, char* argv[])
{
    if (argc != 5)
    {
        printf("Usage: %s <path_b04> <path_b08> <path_b11> <path_scl>\n", argv[0]);
        printf("Example: %s B04.jp2 B08.jp2 B11.jp2 SCL.jp2\n", argv[0]);
        return 1;
    }

    GDALAllRegister();
    printf("[%s] GDAL initialized.\n", get_timestamp());

    char* path_b04 = argv[1];
    char* path_b08 = argv[2];
    char* path_b11 = argv[3];
    char* path_scl = argv[4];

    int res_10m_selected = 1;

    printf("[%s] Starting memory leak test with files:\n", get_timestamp());
    printf("  B04: %s\n", path_b04);
    printf("  B08: %s\n", path_b08);
    printf("  B11: %s\n", path_b11);
    printf("  SCL: %s\n", path_scl);
    printf("  Resolution: %s\n", res_10m_selected ? "10m (upscaling)" : "20m (downscaling)");

    ProcessingResult* result = extract_processing_logic(path_b04, path_b08, path_b11, path_scl, res_10m_selected);

    if (result)
    {
        printf("[%s] Processing successful. Result dimensions: %dx%d\n",
               get_timestamp(), result->width, result->height);
        printf("[%s] NDVI data pointer: %p\n", get_timestamp(), (void*)result->ndvi_data);
        printf("[%s] NDMI data pointer: %p\n", get_timestamp(), (void*)result->ndmi_data);

        extract_cleanup_logic(result);
    }
    else
    {
        printf("[%s] Processing failed.\n", get_timestamp());
    }

    GDALDestroyDriverManager();
    printf("[%s] GDAL cleaned up.\n", get_timestamp());

    printf("[%s] Memory leak test completed.\n", get_timestamp());
    return result ? 0 : 1;
}
