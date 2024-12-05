#include "image_helpers.h"
#include <string.h>
#include <stdlib.h>
#include "logger.h"

ErrorCode image_helper_attach_image(Tcl_Interp* interp, 
                                  const char* label_path,
                                  const char* image_path, 
                                  int orientation_angle) {
    char cmd[512];
    
    // Load and process image using Tcl/Tk image commands
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