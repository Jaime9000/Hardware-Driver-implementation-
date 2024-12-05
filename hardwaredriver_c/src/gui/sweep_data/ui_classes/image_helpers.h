#ifndef IMAGE_HELPERS_H
#define IMAGE_HELPERS_H

#include <tcl.h>
#include <tk.h>
#include "error_codes.h"

// Function to attach an image to a label with rotation
ErrorCode image_helper_attach_image(Tcl_Interp* interp, 
                                  const char* label_path,
                                  const char* image_path, 
                                  int orientation_angle);

#endif // IMAGE_HELPERS_H