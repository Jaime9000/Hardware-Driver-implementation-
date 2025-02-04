#ifndef PICTURE_WINDOW_FUNCTIONS_H
#define PICTURE_WINDOW_FUNCTIONS_H

// System headers
#include <tcl.h>
#include <tk.h>
#include <stdbool.h>

// Project headers
#include "src/core/error_codes.h"
#include "src/gui/sweep_data/tilt_windows/namespace_options.h"

// Constants
#define DEFAULT_IMAGES_DIR "C:\\hardwaredriver_c\\src\\data"
#define DEFAULT_FRONTAL_IMAGE "figure8.jpg"
#define DEFAULT_SAGITTAL_IMAGE "figure10.jpg"

// Forward declaration
typedef struct PictureWindowFunctions PictureWindowFunctions;

// Constructor/Destructor
/**
 * Creates a new picture window functions instance
 * @param interp Tcl interpreter
 * @param is_frontal True if this is a frontal view window
 * @param patient_name Patient name in format "lastname+firstname"
 * @param window_path Tcl path to the window
 * @return New PictureWindowFunctions instance or NULL on failure
 */
PictureWindowFunctions* picture_window_functions_create(Tcl_Interp* interp, 
                                                      bool is_frontal,
                                                      const char* patient_name,
                                                      const char* window_path);

/**
 * Destroys a picture window functions instance
 * @param window Instance to destroy
 */
void picture_window_functions_destroy(PictureWindowFunctions* window);

// Window management methods
ErrorCode picture_window_functions_update_patient_name(PictureWindowFunctions* window, 
                                                     const char* patient_name);
ErrorCode picture_window_functions_create_image_handler(PictureWindowFunctions* window);
ErrorCode picture_window_functions_place_window(PictureWindowFunctions* window);

// Drawing methods
ErrorCode picture_window_functions_draw_line_at_angle(PictureWindowFunctions* window,
                                                     const char* canvas_path,
                                                     double angle, 
                                                     int size);
char* picture_window_functions_pad_values(double angle);

// Getters
const char* picture_window_functions_get_image_path(PictureWindowFunctions* window);
int picture_window_functions_get_size(PictureWindowFunctions* window);

#endif // PICTURE_WINDOW_FUNCTIONS_H