#ifndef IMAGE_WINDOW_OPTIONS_H
#define IMAGE_WINDOW_OPTIONS_H

typedef struct {
    double max_frontal;
    double min_frontal;
    double max_sagittal;
    double min_sagittal;
    double current_frontal;
    double current_sagittal;
} ImageWindowOptions;

void image_window_options_reset(ImageWindowOptions* options);

#endif // IMAGE_WINDOW_OPTIONS_H