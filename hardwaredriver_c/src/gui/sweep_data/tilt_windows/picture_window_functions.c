#include "picture_window_functions.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "windows_api.h"
#include "logger.h"

#define DEFAULT_IMAGES_DIR "C:\\hardwaredriver_c\\src\\data"
#define DEFAULT_FRONTAL_IMAGE "figure8.jpg"
#define DEFAULT_SAGITTAL_IMAGE "figure10.jpg"

struct PictureWindowFunctions {
    Tcl_Interp* interp;
    bool is_frontal;
    char* patient_name;
    char* window_path;
    char* filename_face;
    int size;
};

PictureWindowFunctions* picture_window_functions_create(Tcl_Interp* interp,
                                                      bool is_frontal,
                                                      const char* patient_name,
                                                      const char* window_path) {
    PictureWindowFunctions* window = calloc(1, sizeof(PictureWindowFunctions));
    if (!window) return NULL;

    window->interp = interp;
    window->is_frontal = is_frontal;
    window->patient_name = strdup(patient_name);
    window->window_path = strdup(window_path);
    window->size = 100;  // Default size

    // Update patient name will set filename_face
    if (picture_window_functions_update_patient_name(window, patient_name) != ERROR_NONE) {
        picture_window_functions_destroy(window);
        return NULL;
    }

    // Setup window properties
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "%s overrideredirect 1; "
             "%s attributes -topmost 1",
             window_path, window_path);
    if (Tcl_Eval(interp, cmd) != TCL_OK) {
        picture_window_functions_destroy(window);
        return NULL;
    }

    // Add watch event setup like Python version
    ErrorCode result = setup_watch_event((RedrawCallback)picture_window_functions_place_window);
    if (result != ERROR_NONE) {
        picture_window_functions_destroy(window);
        return NULL;
    }

    return window;
}

ErrorCode picture_window_functions_update_patient_name(PictureWindowFunctions* window,
                                                     const char* patient_name) {
    if (!window || !patient_name) return ERROR_INVALID_PARAMETER;

    // Split patient name
    char* name_copy = strdup(patient_name);
    char* last_name = strchr(name_copy, '+');
    if (!last_name) {
        free(name_copy);
        return ERROR_INVALID_PARAMETER;
    }
    *last_name++ = '\0';
    char* first_name = name_copy;

    // Build patient-specific image path
    char patient_image_path[MAX_PATH];
    snprintf(patient_image_path, sizeof(patient_image_path), "%s\\%s\\%s\\%s",
             namespace_options_get_root_data_dir(),
             last_name,
             first_name,
             window->is_frontal ? "sagittal.JPG" : "frontal.JPG");

    // Check if patient-specific image exists
    if (GetFileAttributesA(patient_image_path) != INVALID_FILE_ATTRIBUTES) {
        // Use patient image
        free(window->filename_face);
        window->filename_face = strdup(patient_image_path);
    } else {
        // Use default image
        char default_image_path[MAX_PATH];
        snprintf(default_image_path, sizeof(default_image_path), "%s\\%s",
                DEFAULT_IMAGES_DIR,
                window->is_frontal ? DEFAULT_FRONTAL_IMAGE : DEFAULT_SAGITTAL_IMAGE);
        
        free(window->filename_face);
        window->filename_face = strdup(default_image_path);
    }

    free(name_copy);
    
    // Update window title with patient name
    free(window->patient_name);
    window->patient_name = strdup(patient_name);

    return ERROR_NONE;
}

ErrorCode picture_window_functions_create_image_handler(PictureWindowFunctions* window) {
    char cmd[512];
    const char* line_color = window->is_frontal ? "blue" : "red";
    
    snprintf(cmd, sizeof(cmd),
             "image create photo tmp_img -file {%s}; "
             "image create photo resized_img; "
             "resized_img copy tmp_img -subsample [expr {[image width tmp_img]/%d}] "
             "[expr {[image height tmp_img]/%d}]; "
             "%s create line %d 0 %d %d -width 3 -fill %s; "
             "image delete tmp_img",
             window->filename_face, 
             window->size, window->size,
             window->window_path,
             window->size/2, window->size/2, window->size,
             line_color);
             
    return Tcl_Eval(window->interp, cmd) == TCL_OK ? ERROR_NONE : ERROR_TCL_EVAL;
}

ErrorCode picture_window_functions_place_window(PictureWindowFunctions* window) {
    int x, y, size;
    ErrorCode err = load_placement_values(window->is_frontal, &x, &y, &size);
    if (err != ERROR_NONE) return err;

    window->size = size;
    
    err = picture_window_functions_create_image_handler(window);
    if (err != ERROR_NONE) return err;
    
    char cmd[256];
    snprintf(cmd, sizeof(cmd), 
             "%s geometry %dx%d+%d+%d",
             window->window_path, size, size + 20, x, y);
    
    return Tcl_Eval(window->interp, cmd) == TCL_OK ? ERROR_NONE : ERROR_TCL_EVAL;
}

ErrorCode picture_window_functions_draw_line_at_angle(PictureWindowFunctions* window,
                                                     const char* canvas_path,
                                                     double angle, 
                                                     int size) {
    int x = (angle < 0) ? size : 0;
    if (angle < 0) angle *= -1;
    
    double y = size - (tan(M_PI/2 - angle*M_PI/180) * size/2);
    
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "%s create line %d %d %d %f -width 3 -fill grey",
             canvas_path, size/2, size, x, y);
             
    return Tcl_Eval(window->interp, cmd) == TCL_OK ? ERROR_NONE : ERROR_TCL_EVAL;
}

char* picture_window_functions_pad_values(double angle) {
    char* result = malloc(32);
    if (!result) return NULL;
    
    int space_count = 0;
    if (angle > 0) space_count++;
    if (fabs(angle) < 10) space_count++;
    
    snprintf(result, 32, "%*s%d", space_count*2, "", (int)angle);
    return result;
}

void picture_window_functions_destroy(PictureWindowFunctions* window) {
    if (!window) return;
    
    // Stop the watch event before cleanup
    stop_watch_event();
    
    free(window->patient_name);
    free(window->window_path);
    free(window->filename_face);
    free(window);
}