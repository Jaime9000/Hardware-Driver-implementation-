#ifndef PICTURE_WINDOW_FUNCTIONS_H
#define PICTURE_WINDOW_FUNCTIONS_H
#define DEFAULT_IMAGES_DIR "C:\\hardwaredriver_c\\src\\data"
#define DEFAULT_FRONTAL_IMAGE "figure8.jpg"
#define DEFAULT_SAGITTAL_IMAGE "figure10.jpg"

#include <tcl.h>
#include <tk.h>
#include "error_codes.h"
#include "namespace_options.h"

typedef struct PictureWindowFunctions PictureWindowFunctions;

// Constructor/Destructor
PictureWindowFunctions* picture_window_functions_create(Tcl_Interp* interp, 
                                                      bool is_frontal,
                                                      const char* patient_name,
                                                      const char* window_path);
void picture_window_functions_destroy(PictureWindowFunctions* window);

// Methods
ErrorCode picture_window_functions_update_patient_name(PictureWindowFunctions* window, 
                                                     const char* patient_name);
ErrorCode picture_window_functions_create_image_handler(PictureWindowFunctions* window);
ErrorCode picture_window_functions_place_window(PictureWindowFunctions* window);
ErrorCode picture_window_functions_draw_line_at_angle(PictureWindowFunctions* window,
                                                     const char* canvas_path,
                                                     double angle, 
                                                     int size);
char* picture_window_functions_pad_values(double angle);

// Getters
const char* picture_window_functions_get_image_path(PictureWindowFunctions* window);
int picture_window_functions_get_size(PictureWindowFunctions* window);



#endif // PICTURE_WINDOW_FUNCTIONS_H