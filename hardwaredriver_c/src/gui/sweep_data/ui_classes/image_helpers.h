#ifndef IMAGE_HELPERS_H
#define IMAGE_HELPERS_H

// System headers
#include <tcl.h>
#include <tk.h>

// Project headers
#include "src/core/error_codes.h"

/**
 * @brief Attaches an image to a Tk label with rotation and resizing
 * 
 * This function performs the following operations:
 * 1. Loads the image from the specified path
 * 2. Resizes the image by subsampling to 1/50th of original size
 * 3. Rotates the image by the specified angle
 * 4. Attaches the processed image to the specified label
 * 5. Cleans up temporary images
 *
 * @param interp Tcl interpreter instance
 * @param label_path Path to the Tk label widget
 * @param image_path Path to the image file
 * @param orientation_angle Rotation angle in degrees
 * @return ERROR_NONE on success, ERROR_TCL_EVAL on failure
 */
ErrorCode image_helper_attach_image(Tcl_Interp* interp, 
                                  const char* label_path,
                                  const char* image_path, 
                                  int orientation_angle);

#endif // IMAGE_HELPERS_H