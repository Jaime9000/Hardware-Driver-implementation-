#include "src/gui/sweep_data/tilt_windows/image_window_options.h"

void image_window_options_reset(ImageWindowOptions* options) {
    if (!options) return;
    options->max_frontal = 0;
    options->min_frontal = 0;
    options->max_sagittal = 0;
    options->min_sagittal = 0;
}