#include <string.h>
#include <stdlib.h>

#include "src/gui/sweep_data/ui_classes/image_helpers.h"
#include "src/core/logger.h"

ErrorCode image_helper_attach_image(Tcl_Interp* interp, 
                                  const char* label_path,
                                  const char* image_path, 
                                  int orientation_angle) {
    char cmd[512];
    
    // Load and process image using Tcl/Tk image commands
    // Build Tcl command to:
    // 1. Create temporary image from file
    // 2. Create resized image
    // 3. Subsample original image
    // 4. Create rotated image
    // 5. Rotate resized image
    // 6. Configure label with final image
    // 7. Cleanup temporary images
    snprintf(cmd, sizeof(cmd),
             "image create photo tmp_img -file {%s}; "
             "image create photo resized_img; "
             "resized_img copy tmp_img -subsample [expr {[image width tmp_img]/50}] "
             "[expr {[image height tmp_img]/50}]; "
             "image create photo rotated_img; "
             "resized_img rotate rotated_img %d; "
             "%s configure -image rotated_img; "
             "image delete tmp_img resized_img",
             image_path, orientation_angle, label_path);
             
    if (Tcl_Eval(interp, cmd) != TCL_OK) {
        log_error("Failed to process and attach image");
        return ERROR_TCL_EVAL;
    }
    
    return ERROR_NONE;
}