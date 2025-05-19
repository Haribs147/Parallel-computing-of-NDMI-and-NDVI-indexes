#ifndef RESAMPLER_H
#define RESAMPLER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

float* nearest_neighbor_resample_scl(
    const float* input_band,
    int input_width,
    int input_height,
    int output_width,
    int output_height
);

float* bilinear_resample_float(
    const float* input_band,
    int input_width,
    int input_height,
    int output_width,
    int output_height
);

float* average_resample_float(
    const float* input_band,
    int input_width,
    int input_height,
    int output_width,
    int output_height
);

#endif
