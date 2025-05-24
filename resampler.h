#ifndef RESAMPLER_H
#define RESAMPLER_H

#include <gtk/gtk.h>
#include <glib/gtypes.h>

#include "data_types.h"

int resample_all_bands_to_target_resolution(BandData* bands, int band_count, gboolean target_resolution_10m);

#endif
