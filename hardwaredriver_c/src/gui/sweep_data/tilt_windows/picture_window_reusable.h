#ifndef PICTURE_WINDOW_REUSABLE_H
#define PICTURE_WINDOW_REUSABLE_H

// System headers
#include <tcl.h>
#include <tk.h>

// Project headers
#include "src/core/error_codes.h"
#include "src/gui/sweep_data/tilt_windows/picture_window_functions.h"
#include "src/gui/sweep_data/tilt_windows/image_window_options.h"
#include "src/gui/sweep_data/tilt_windows/namespace_options.h"

// Forward declaration
typedef struct PictureWindowReusable PictureWindowReusable;

// Constructor/Destructor
/**
 * Creates a new reusable picture window instance
 * @param interp Tcl interpreter
 * @param patient_name Patient name in format "lastname+firstname"
 * @param name_window_path Tcl path for the window
 * @param is_frontal True if this is a frontal view window
 * @param options Pointer to window display options
 * @return New PictureWindowReusable instance or NULL on failure
 */
PictureWindowReusable* picture_window_reusable_create(Tcl_Interp* interp,
                                                     const char* patient_name,
                                                     const char* name_window_path,
                                                     bool is_frontal,
                                                     ImageWindowOptions* options);

/**
 * Destroys a reusable picture window instance
 * @param window Instance to destroy
 */
void picture_window_reusable_destroy(PictureWindowReusable* window);

// Window control methods
/**
 * Starts the window update timer and shows the window
 * @param window Instance to start
 * @return ERROR_NONE on success, error code otherwise
 */
ErrorCode picture_window_reusable_start(PictureWindowReusable* window);

/**
 * Stops the window update timer and hides the window
 * @param window Instance to stop
 * @return ERROR_NONE on success, error code otherwise
 */
ErrorCode picture_window_reusable_stop(PictureWindowReusable* window);

// Window state methods
/**
 * Updates the patient name and associated image paths
 * @param window Instance to update
 * @param new_patient_name New patient name in format "lastname+firstname"
 * @return ERROR_NONE on success, error code otherwise
 */
ErrorCode picture_window_reusable_update_patient_name(PictureWindowReusable* window, 
                                                    const char* new_patient_name);

/**
 * Hides the window without stopping updates
 * @param window Instance to hide
 * @return ERROR_NONE on success, error code otherwise
 */
ErrorCode picture_window_reusable_hide_window(PictureWindowReusable* window);

/**
 * Shows the window if it was hidden
 * @param window Instance to show
 * @return ERROR_NONE on success, error code otherwise
 */
ErrorCode picture_window_reusable_show_window(PictureWindowReusable* window);

/**
 * Opens a file dialog to load a new image
 * @param window Instance to update
 * @return ERROR_NONE on success, error code otherwise
 */
ErrorCode picture_window_reusable_load_image(PictureWindowReusable* window);

#endif // PICTURE_WINDOW_REUSABLE_H