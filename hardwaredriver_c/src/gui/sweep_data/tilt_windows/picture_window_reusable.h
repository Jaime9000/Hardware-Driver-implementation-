#ifndef PICTURE_WINDOW_REUSABLE_H
#define PICTURE_WINDOW_REUSABLE_H

#include <tcl.h>
#include <tk.h>
#include "error_codes.h"
#include "picture_window_functions.h"
#include "image_window_options.h"
#include "namespace_options.h"

typedef struct PictureWindowReusable PictureWindowReusable;

// Constructor/Destructor
PictureWindowReusable* picture_window_reusable_create(Tcl_Interp* interp,
                                                     const char* patient_name,
                                                     const char* name_window_path,
                                                     bool is_frontal,
                                                     ImageWindowOptions* options);
void picture_window_reusable_destroy(PictureWindowReusable* window);

// Public Methods
ErrorCode picture_window_reusable_start(PictureWindowReusable* window);
ErrorCode picture_window_reusable_stop(PictureWindowReusable* window);
ErrorCode picture_window_reusable_update_patient_name(PictureWindowReusable* window, 
                                                    const char* new_patient_name);
ErrorCode picture_window_reusable_hide_window(PictureWindowReusable* window);
ErrorCode picture_window_reusable_show_window(PictureWindowReusable* window);
ErrorCode picture_window_reusable_load_image(PictureWindowReusable* window);

#endif // PICTURE_WINDOW_REUSABLE_H