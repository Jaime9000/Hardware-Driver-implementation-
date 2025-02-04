#ifndef IMAGE_WINDOW_OPTIONS_H
#define IMAGE_WINDOW_OPTIONS_H

// Structure to hold window display options for tilt images
typedef struct {
    double max_frontal;      // Maximum frontal tilt angle
    double min_frontal;      // Minimum frontal tilt angle
    double max_sagittal;     // Maximum sagittal tilt angle
    double min_sagittal;     // Minimum sagittal tilt angle
    double current_frontal;  // Current frontal tilt angle
    double current_sagittal; // Current sagittal tilt angle
} ImageWindowOptions;

/**
 * Resets all options to their default values (zero)
 * @param options Pointer to ImageWindowOptions structure to reset
 */
void image_window_options_reset(ImageWindowOptions* options);

#endif // IMAGE_WINDOW_OPTIONS_H