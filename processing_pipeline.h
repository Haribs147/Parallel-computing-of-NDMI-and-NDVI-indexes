#ifndef PROCESSING_PIPELINE_H
#define PROCESSING_PIPELINE_H

#include "gui_utils.h"
#include <stdbool.h>

typedef struct
{
    float* ndvi_data;
    float* ndmi_data;
    int width;
    int height;
} ProcessingResult;

ProcessingResult* process_bands_and_calculate_indices(BandData bands[4], bool target_10m);

#endif // PROCESSING_PIPELINE_H
