#include "src/gui/sweep_data/tilt_windows/picture_window_reusable.h"
#include "src/core/logger.h"
#include <string.h>
#include <stdlib.h>
#include <windows.h>
#include <math.h>

struct PictureWindowReusable {
    PictureWindowFunctions* base;
    Tcl_Interp* interp;
    char* window_path;
    char* canvas_path;
    char* frame_path;
    char* max_label_path;
    char* current_label_path;
    char* min_label_path;
    char* patient_name;
    bool is_frontal;
    bool is_running;
    bool window_hidden;
    ImageWindowOptions* image_options;
    CRITICAL_SECTION mutex;
    NamespaceOptions* namespace;
};

static ErrorCode create_ui_elements(PictureWindowReusable* window) {
    if (!window) return ERROR_INVALID_PARAMETER;

    char cmd[1024];
    
    // Create canvas with click binding
    snprintf(cmd, sizeof(cmd),
             "canvas %s -width 100 -height 100; "
             "pack %s -side top; "
             "bind %s <Button-1> {LoadImage %d}",
             window->canvas_path, window->canvas_path, 
             window->canvas_path, window->is_frontal);
    if (Tcl_Eval(window->interp, cmd) != TCL_OK) {
        return ERROR_TCL_EVAL;
    }

    // Create initial image in canvas
    ErrorCode err = picture_window_functions_create_image_handler(window->base);
    if (err != ERROR_NONE) {
        return err;
    }

    // Create frame
    snprintf(cmd, sizeof(cmd),
             "frame %s -bd 1 -relief sunken; "
             "pack %s -side bottom -fill x",
             window->frame_path, window->frame_path);
    if (Tcl_Eval(window->interp, cmd) != TCL_OK) {
        return ERROR_TCL_EVAL;
    }

    // Create labels
    snprintf(cmd, sizeof(cmd),
             "label %s; pack %s -side left -anchor w -fill x; "
             "label %s; pack %s -side left -anchor center -fill x; "
             "label %s; pack %s -side right -anchor e -fill x",
             window->max_label_path, window->max_label_path,
             window->current_label_path, window->current_label_path,
             window->min_label_path, window->min_label_path);
    if (Tcl_Eval(window->interp, cmd) != TCL_OK) {
        return ERROR_TCL_EVAL;
    }

    return ERROR_NONE;
}

static void draw_callback(ClientData client_data) {
    PictureWindowReusable* window = (PictureWindowReusable*)client_data;
    
    EnterCriticalSection(&window->mutex);
    if (!window->is_running) {
        LeaveCriticalSection(&window->mutex);
        return;
    }

    // Handle window visibility based on namespace options
    if (window->namespace->options_display) {
        if (window->window_hidden) {
            char cmd[256];
            snprintf(cmd, sizeof(cmd), "wm deiconify %s", window->window_path);
            Tcl_Eval(window->interp, cmd);
            window->window_hidden = false;
        }
    } else {
        if (!window->window_hidden) {
            char cmd[256];
            snprintf(cmd, sizeof(cmd), "wm withdraw %s", window->window_path);
            Tcl_Eval(window->interp, cmd);
            window->window_hidden = true;
        }
    }

    int size = picture_window_functions_get_size(window->base);
    char cmd[1024];

    // Handle angle drawing and labels
    if (window->image_options->max_frontal != 0) {  // Check if we have angle limits
        // Draw max and min angle lines
        double max_angle, min_angle;
        const char* max_label;
        const char* min_label;
        
        if (window->is_frontal) {
            max_angle = window->image_options->max_frontal;
            min_angle = window->image_options->min_frontal;
            max_label = "A Flex";
            min_label = "P Ext";
        } else {
            max_angle = window->image_options->max_sagittal;
            min_angle = window->image_options->min_sagittal;
            max_label = "R Flex";
            min_label = "L Flex";
        }

        // Clear canvas and redraw base image
        snprintf(cmd, sizeof(cmd), "%s delete all", window->canvas_path);
        Tcl_Eval(window->interp, cmd);
        picture_window_functions_create_image_handler(window->base);

        // Draw the lines
        picture_window_functions_draw_line_at_angle(window->base, window->canvas_path, 
                                                  max_angle, size);
        picture_window_functions_draw_line_at_angle(window->base, window->canvas_path, 
                                                  min_angle, size);

        // Update labels
        int max_text = (int)fabs(max_angle);
        int min_text = (int)fabs(min_angle);

        snprintf(cmd, sizeof(cmd),
                "%s configure -text \"%s%d°\"; "
                "%s configure -text \"%s%d°\"; "
                "%s configure -text \"\"",
                window->max_label_path, max_label, max_text,
                window->min_label_path, min_label, min_text,
                window->current_label_path);
    } else {
        // Clear max/min labels and show current angle
        double current_angle = window->is_frontal ? 
            window->image_options->current_frontal : 
            window->image_options->current_sagittal;

        char* padded_value = picture_window_functions_pad_values(current_angle);
        if (padded_value) {
            snprintf(cmd, sizeof(cmd),
                    "%s configure -text \"\"; "
                    "%s configure -text \"\"; "
                    "%s configure -text \"Angle  %s°\"",
                    window->max_label_path,
                    window->min_label_path,
                    window->current_label_path,
                    padded_value);
            free(padded_value);

            // Clear canvas and create rotated image
            //this roation command needs to be checked that its roatating the correct way/direction
            snprintf(cmd + strlen(cmd), sizeof(cmd) - strlen(cmd),
                    "; %s delete all; "
                    "%s create image %d %d -image [image create photo -file {%s} -rotate %f]",
                    window->canvas_path,
                    window->canvas_path,
                    size/2, size/2,
                    picture_window_functions_get_image_path(window->base),
                    current_angle);
        }
    }

    // Execute the Tcl commands
    if (Tcl_Eval(window->interp, cmd) != TCL_OK) {
        log_error("Failed to execute draw commands");
    }

    // Configure canvas size
    snprintf(cmd, sizeof(cmd),
             "%s configure -width %d -height %d -bg white",
             window->canvas_path, size, size);
    Tcl_Eval(window->interp, cmd);

    // Schedule next draw
    Tcl_CreateTimerHandler(100, draw_callback, window);
    
    LeaveCriticalSection(&window->mutex);
}

PictureWindowReusable* picture_window_reusable_create(Tcl_Interp* interp,
                                                     const char* patient_name,
                                                     const char* name_window_path,
                                                     bool is_frontal,
                                                     ImageWindowOptions* options) {
    PictureWindowReusable* window = calloc(1, sizeof(PictureWindowReusable));
    if (!window) return NULL;
    
    InitializeCriticalSection(&window->mutex);
    
    // Create window title
    char title[256];
    snprintf(title, sizeof(title), "Picture window: %s", is_frontal ? "Frontal" : "Sagittal");
    
    // Create toplevel window
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "toplevel %s; wm title %s {%s}", 
             name_window_path, name_window_path, title);
    if (Tcl_Eval(interp, cmd) != TCL_OK) {
        picture_window_reusable_destroy(window);
        return NULL;
    }

    // Initialize base PictureWindowFunctions
    window->base = picture_window_functions_create(interp, is_frontal, patient_name, name_window_path);
    if (!window->base) {
        picture_window_reusable_destroy(window);
        return NULL;
    }

    window->interp = interp;
    window->is_frontal = is_frontal;
    window->patient_name = strdup(patient_name);
    window->window_path = strdup(name_window_path);
    window->image_options = options;
    window->is_running = false;
    window->window_hidden = false;

    // Create paths for UI elements
    size_t path_len = strlen(name_window_path) + 32;
    window->canvas_path = malloc(path_len);
    window->frame_path = malloc(path_len);
    window->max_label_path = malloc(path_len);
    window->current_label_path = malloc(path_len);
    window->min_label_path = malloc(path_len);
    
    if (!window->canvas_path || !window->frame_path || 
        !window->max_label_path || !window->current_label_path || !window->min_label_path) {
        picture_window_reusable_destroy(window);
        return NULL;
    }

    snprintf(window->canvas_path, path_len, "%s.canvas", name_window_path);
    snprintf(window->frame_path, path_len, "%s.frame", name_window_path);
    snprintf(window->max_label_path, path_len, "%s.frame.max_label", name_window_path);
    snprintf(window->current_label_path, path_len, "%s.frame.current_label", name_window_path);
    snprintf(window->min_label_path, path_len, "%s.frame.min_label", name_window_path);

    // Create UI elements
    if (create_ui_elements(window) != ERROR_NONE) {
        picture_window_reusable_destroy(window);
        return NULL;
    }

    // Setup namespace options
    window->namespace = calloc(1, sizeof(NamespaceOptions));
    if (!window->namespace || 
        namespace_options_create(&window->namespace, false) != ERROR_NONE ||
        namespace_options_setup_watch(window->namespace, NULL, NULL, NULL) != ERROR_NONE) {
        picture_window_reusable_destroy(window);
        return NULL;
    }

    return window;
}

ErrorCode picture_window_reusable_start(PictureWindowReusable* window) {
    if (!window) return ERROR_INVALID_PARAMETER;
    
    EnterCriticalSection(&window->mutex);
    if (!window->is_running) {
        window->is_running = true;
        Tcl_CreateTimerHandler(100, draw_callback, window);
        picture_window_reusable_show_window(window);
    }
    LeaveCriticalSection(&window->mutex);
    return ERROR_NONE;
}

ErrorCode picture_window_reusable_stop(PictureWindowReusable* window) {
    if (!window) return ERROR_INVALID_PARAMETER;
    
    EnterCriticalSection(&window->mutex);
    window->is_running = false;
    picture_window_reusable_hide_window(window);
    LeaveCriticalSection(&window->mutex);
    return ERROR_NONE;
}

ErrorCode picture_window_reusable_update_patient_name(PictureWindowReusable* window, 
                                                    const char* new_patient_name) {
    if (!window || !new_patient_name) return ERROR_INVALID_PARAMETER;
    
    EnterCriticalSection(&window->mutex);
    
    // Update base window patient name
    ErrorCode result = picture_window_functions_update_patient_name(window->base, new_patient_name);
    if (result == ERROR_NONE) {
        free(window->patient_name);
        window->patient_name = strdup(new_patient_name);
    }
    
    LeaveCriticalSection(&window->mutex);
    return result;
}

ErrorCode picture_window_reusable_hide_window(PictureWindowReusable* window) {
    if (!window) return ERROR_INVALID_PARAMETER;
    
    EnterCriticalSection(&window->mutex);
    if (!window->window_hidden) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "wm withdraw %s", window->window_path);
        if (Tcl_Eval(window->interp, cmd) != TCL_OK) {
            LeaveCriticalSection(&window->mutex);
            return ERROR_TCL_EVAL;
        }
        window->window_hidden = true;
    }
    LeaveCriticalSection(&window->mutex);
    return ERROR_NONE;
}

ErrorCode picture_window_reusable_show_window(PictureWindowReusable* window) {
    if (!window) return ERROR_INVALID_PARAMETER;
    
    EnterCriticalSection(&window->mutex);
    if (window->window_hidden) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "wm deiconify %s", window->window_path);
        if (Tcl_Eval(window->interp, cmd) != TCL_OK) {
            LeaveCriticalSection(&window->mutex);
            return ERROR_TCL_EVAL;
        }
        window->window_hidden = false;
    }
    LeaveCriticalSection(&window->mutex);
    return ERROR_NONE;
}

ErrorCode picture_window_reusable_load_image(PictureWindowReusable* window) {
    if (!window) return ERROR_INVALID_PARAMETER;
    
    EnterCriticalSection(&window->mutex);
    
    char cmd[1024];
    // Open file dialog with image file types
    snprintf(cmd, sizeof(cmd),
             "tk_getOpenFile "
             "-title {Open a file} "
             "-initialdir / "
             "-filetypes {{Image Files {.jpg}} {Image Files {.png}}}");
    
    if (Tcl_Eval(window->interp, cmd) != TCL_OK) {
        LeaveCriticalSection(&window->mutex);
        return ERROR_TCL_EVAL;
    }
    
    const char* filename = Tcl_GetStringResult(window->interp);
    if (strlen(filename) > 0) {
        // Split patient name
        char* name_copy = strdup(window->patient_name);
        char* last_name = strchr(name_copy, '+');
        if (!last_name) {
            free(name_copy);
            LeaveCriticalSection(&window->mutex);
            return ERROR_INVALID_PARAMETER;
        }
        *last_name++ = '\0';
        char* first_name = name_copy;

        // Build destination path
        char dest_path[MAX_PATH];
        snprintf(dest_path, sizeof(dest_path), "%s\\%s\\%s\\%s",
                namespace_options_get_root_data_dir(),
                last_name, first_name,
                window->is_frontal ? "sagittal.JPG" : "frontal.JPG");

        // Copy file
        if (!CopyFileA(filename, dest_path, FALSE)) {
            free(name_copy);
            LeaveCriticalSection(&window->mutex);
            return ERROR_FILE_COPY;
        }
        free(name_copy);

        // Update image
        ErrorCode result = picture_window_functions_create_image_handler(window->base);
        LeaveCriticalSection(&window->mutex);
        return result;
    }
    
    LeaveCriticalSection(&window->mutex);
    return ERROR_NONE;  // User cancelled file selection
}

void picture_window_reusable_destroy(PictureWindowReusable* window) {
    if (!window) return;

    EnterCriticalSection(&window->mutex);
    window->is_running = false;
    LeaveCriticalSection(&window->mutex);

    if (window->base) {
        picture_window_functions_destroy(window->base);
    }

    if (window->namespace) {
        namespace_options_destroy(window->namespace);
    }

    free(window->patient_name);
    free(window->window_path);
    free(window->canvas_path);
    free(window->frame_path);
    free(window->max_label_path);
    free(window->current_label_path);
    free(window->min_label_path);

    DeleteCriticalSection(&window->mutex);
    free(window);
}

