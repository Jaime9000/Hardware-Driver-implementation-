#include "src/gui/sweep_data/sweep_data_reusable_ui.h"
#include "src/core/logger.h"

// System headers
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <shlwapi.h>

// Constants
#define WINDOW_TITLE "K7-Postural Range of Motion"
#define TCL_INIT_SCRIPT "package require Tk\n"
#define TIMER_INTERVAL 100  // milliseconds

// Forward declarations for static functions
static ErrorCode initialize_tcl_tk(PlotAppReusable* app);
static ErrorCode setup_value_labels(PlotAppReusable* app);
static ErrorCode process_command_queue(PlotAppReusable* app);
static void process_command_queue_callback(ClientData client_data);

/**
 * @brief Initializes Tcl/Tk environment
 */
static ErrorCode initialize_tcl_tk(PlotAppReusable* app) {
    if (!app) return ERROR_INVALID_PARAMETER;

    app->interp = Tcl_CreateInterp();
    if (!app->interp) {
        log_error("Failed to create Tcl interpreter");
        return ERROR_TCL_INIT;
    }

    if (Tcl_Init(app->interp) != TCL_OK) {
        log_error("Failed to initialize Tcl: %s", Tcl_GetStringResult(app->interp));
        return ERROR_TCL_INIT;
    }

    if (Tk_Init(app->interp) != TCL_OK) {
        log_error("Failed to initialize Tk: %s", Tcl_GetStringResult(app->interp));
        return ERROR_TCL_INIT;
    }

    // Initialize Tk with basic script
    char init_script[256];
    snprintf(init_script, sizeof(init_script), TCL_INIT_SCRIPT);
    if (Tcl_Eval(app->interp, init_script) != TCL_OK) {
        log_error("Failed to run initialization script: %s", Tcl_GetStringResult(app->interp));
        return ERROR_TCL_EVAL;
    }

    // Create the UI
    create_ui(app->interp);

    // Now set window properties
    char window_cmd[256];
    snprintf(window_cmd, sizeof(window_cmd), 
        "wm title .mainwindow {%s}\n"
        "wm protocol .mainwindow WM_DELETE_WINDOW {exit}",
        WINDOW_TITLE);
    if (Tcl_Eval(app->interp, window_cmd) != TCL_OK) {
        log_error("Failed to set window properties: %s", Tcl_GetStringResult(app->interp));
        return ERROR_TCL_EVAL;
    }

    return ERROR_NONE;
}


static ErrorCode setup_value_labels(PlotAppReusable* app) {
    // Create label widgets
    const char* create_labels_cmd =
        "label .container.controls.a_flex_label\n"
        "label .container.controls.p_ext_label\n"
        "label .container.controls.r_flex_label\n"
        "label .container.controls.l_flex_label\n"
        "label .container.controls.ap_pitch_label -text \"A/P Pitch\" -font {Arial 12 bold}\n"
        "label .container.controls.lateral_roll_label -text \"Lateral Roll\" -font {Arial 12 bold}\n";

    if (Tcl_Eval(app->interp, create_labels_cmd) != TCL_OK) {
        log_error("Failed to create label widgets");
        return ERROR_TCL_EVAL;
    }

    // Store widget paths
    app->a_flex_label = strdup(".container.controls.a_flex_label");
    app->p_ext_label = strdup(".container.controls.p_ext_label");
    app->r_flex_label = strdup(".container.controls.r_flex_label");
    app->l_flex_label = strdup(".container.controls.l_flex_label");
    app->ap_pitch_label = strdup(".container.controls.ap_pitch_label");
    app->lateral_roll_label = strdup(".container.controls.lateral_roll_label");

    // Load and attach images
    char image_path[MAX_PATH_LENGTH];
    
    // Blue arrow
    PathCombine(image_path, app->resource_path, "images/blue_arrow.jpg");
    ErrorCode error = image_helper_attach_image(app->interp, app->a_flex_label, image_path, 90);
    if (error != ERROR_NONE) return error;

    error = image_helper_attach_image(app->interp, app->p_ext_label, image_path, -90);
    if (error != ERROR_NONE) return error;

    // Red arrow
    PathCombine(image_path, app->resource_path, "images/red_arrow.jpg");
    error = image_helper_attach_image(app->interp, app->r_flex_label, image_path, 90);
    if (error != ERROR_NONE) return error;

    error = image_helper_attach_image(app->interp, app->l_flex_label, image_path, -90);
    if (error != ERROR_NONE) return error;

    // Pack labels
    const char* pack_labels_cmd =
        "pack .container.controls.a_flex_label\n"
        "pack .container.controls.p_ext_label -pady {0 75}\n"
        "pack .container.controls.r_flex_label -pady {45 0}\n"
        "pack .container.controls.l_flex_label\n"
        "pack .container.controls.ap_pitch_label\n"
        "pack .container.controls.lateral_roll_label -pady {230 0}\n";

    if (Tcl_Eval(app->interp, pack_labels_cmd) != TCL_OK) {
        log_error("Failed to pack labels");
        return ERROR_TCL_EVAL;
    }

    return ERROR_NONE;
}

// Constructor implementation
ErrorCode plot_app_reusable_create(PlotAppReusable** app, 
                                 DataQueue* data_queue,
                                 NamespaceOptions* namespace,
                                 DataQueue* command_queue) {
    if (!app || !data_queue || !namespace || !command_queue) {
        return ERROR_INVALID_PARAMETER;
    }

    PlotAppReusable* new_app = (PlotAppReusable*)calloc(1, sizeof(PlotAppReusable));
    if (!new_app) {
        return ERROR_MEMORY_ALLOCATION;
    }

    new_app->data_queue = data_queue;
    new_app->command_queue = command_queue;
    new_app->namespace = namespace;
    new_app->table_filter_callback = plot_app_reusable_table_filter_callback;
    new_app->cms_callback = plot_app_reusable_playback_cms_window;
    new_app->playback_callback = plot_app_reusable_playback_callback;

    new_app->main_window = ".mainwindow";

    // Initialize Tcl/Tk
    ErrorCode error = initialize_tcl_tk(new_app);
    if (error != ERROR_NONE) {
        free(new_app);
        return error;
    }

    // Setup initial patient path
    char first_name[256], last_name[256];
    error = namespace_options_get_patient_name(new_app->namespace, first_name, last_name);
    if (error != ERROR_NONE) {
        Tcl_DeleteInterp(new_app->interp);
        free(new_app);
        return error;
    }

    PathCombine(new_app->patient_path, 
               namespace_options_get_root_dir(new_app->namespace),
               last_name);
    PathAppend(new_app->patient_path, first_name);
    PathAppend(new_app->patient_path, "sweep_data");
    PathAppend(new_app->patient_path, DATA_VERSION);

    // Create directories if they don't exist
    SHCreateDirectoryEx(NULL, new_app->patient_path, NULL);

    // Create DataClass instance
    new_app->data_class = data_class_create(new_app->interp, 
                                          ".mainwindow.mw_fcontainer",  // builder_path
                                          new_app->namespace->path,
                                          new_app->data_queue);
    if (!new_app->data_class) {
        plot_app_reusable_destroy(new_app);
        return ERROR_OUT_OF_MEMORY;
    }

    // Create Graph instance
    new_app->graph = graph_create(new_app->interp,
                                new_app->main_window,
                                ".mainwindow.mw_fcontainer.mw_fmain.main_container_area.graph_canvas",
                                new_app->data_class,
                                new_app->table_filter_callback,
                                new_app->patient_path,
                                new_app->namespace->patient_name,
                                new_app->cms_callback,
                                new_app->namespace);
    if (!new_app->graph) {
        plot_app_reusable_destroy(new_app);
        return ERROR_OUT_OF_MEMORY;
    }

    *app = new_app;
    return ERROR_NONE;
}

ErrorCode plot_app_reusable_table_filter_callback(PlotAppReusable* app, const char* scan_filter_type) {
    return data_table_repopulate_data(app->table, scan_filter_type);
}

// Destructor implementation
void plot_app_reusable_destroy(PlotAppReusable* app) {
    if (!app) return;

    // Free Tcl/Tk widget paths
    free(app->a_flex_label);
    free(app->p_ext_label);
    free(app->r_flex_label);
    free(app->l_flex_label);
    free(app->ap_pitch_label);
    free(app->lateral_roll_label);

    // Cleanup UI components
    if (app->scrollable_frame) {
        scrollable_frame_destroy(app->scrollable_frame);
    }

    // Delete Tcl interpreter (this will cleanup all Tk widgets)
    if (app->interp) {
        Tcl_DeleteInterp(app->interp);
    }

    free(app);
}

// Core functionality implementation
ErrorCode plot_app_reusable_run(PlotAppReusable* app) {
    if (!app || !app->interp) return ERROR_INVALID_PARAMETER;
    Tk_MainLoop();
    return ERROR_NONE;
}

ErrorCode plot_app_reusable_close_window(PlotAppReusable* app) {
    if (!app) return ERROR_INVALID_PARAMETER;
    graph_stop(app->graph);
    
    return ERROR_NONE;
}

ErrorCode plot_app_reusable_setup_table(PlotAppReusable* app) {
    // Create scrollable frame using existing UI structure
    app->scrollable_frame = scrollable_frame_create(app->interp, 
        ".mainwindow.mw_fcontainer.mw_fmain.table_data_frame_container");
    if (!app->scrollable_frame) {
        log_error("Failed to create scrollable frame");
        return ERROR_MEMORY_ALLOCATION;
    }

    ErrorCode error = scrollable_frame_initialize(app->scrollable_frame);
    if (error != ERROR_NONE) {
        log_error("Failed to initialize scrollable frame");
        return error;
    }

    //VERIFY DATA TABLE FILE AFTER THIS,LOOK AT root param, Colm, row init, Observer implementation in C
    // Create the table instance
    error = data_table_create(
        &app->table,
        app->interp,
        app->patient_path,
        app->playback_callback,
        1,  // column
        2,  // row
        NULL  // check_callback
    );
    if (error != ERROR_NONE) {
        log_error("Failed to create data table");
        return error;
    }

    return ERROR_NONE;
}

ErrorCode plot_app_reusable_change_patient_path(PlotAppReusable* app, const char* new_path) {
    if (!app || !new_path) return ERROR_INVALID_PARAMETER;

    // Read serialized patient name
    SerializedData* data = NULL;
    ErrorCode error = deserialize_from_file(new_path, &data);
    if (error != ERROR_NONE) return error;

    char* patient_name = (char*)data->data;
    char first_name[256], last_name[256];
    
    // Split patient name
    char* plus_pos = strchr(patient_name, '+');
    if (plus_pos) {
        size_t first_len = plus_pos - patient_name;
        strncpy(first_name, patient_name, first_len);
        first_name[first_len] = '\0';
        strcpy(last_name, plus_pos + 1);
    }

    // Update path
    PathCombine(app->patient_path, 
               namespace_options_get_root_dir(app->namespace),
               last_name);
    PathAppend(app->patient_path, first_name);
    PathAppend(app->patient_path, "sweep_data");
    PathAppend(app->patient_path, DATA_VERSION);

    // Create directories if they don't exist
    SHCreateDirectoryEx(NULL, app->patient_path, NULL);

    serialized_data_destroy(data);
    return ERROR_NONE;
}

// Playback functionality implementation
ErrorCode plot_app_reusable_playback_callback(PlotAppReusable* app, const char* file_name) {
    if (!app || !file_name) return ERROR_INVALID_PARAMETER;

    char full_path[MAX_PATH_LENGTH];
    PathCombine(full_path, app->patient_path, file_name);

    // Deserialize playback data
    SerializedData* data = NULL;
    ErrorCode error = deserialize_from_file(full_path, &data);
    if (error != ERROR_NONE) {
        log_error("Failed to deserialize playback data");
        return error;
    }

    // Extract saved data array
    if (!data->data) {
        serialized_data_destroy(data);
        return ERROR_INVALID_DATA;
    }

    // Start playback using data class
    if (app->data_class) {
        error = data_class_start_playback(app->data_class, data->data);
    }

    serialized_data_destroy(data);
    return error;
}

ErrorCode plot_app_reusable_playback_cms_window(PlotAppReusable* app, 
                                              const char* extra_filter,
                                              bool with_summary,
                                              bool fast_replay) {
    if (!app) return ERROR_INVALID_PARAMETER;

    WIN32_FIND_DATA find_data;
    char search_path[MAX_PATH_LENGTH];
    PathCombine(search_path, app->patient_path, "*");

    HANDLE find_handle = FindFirstFile(search_path, &find_data);
    if (find_handle == INVALID_HANDLE_VALUE) {
        return ERROR_FILE_NOT_FOUND;
    }

    do {
        // Skip "." and ".." directories
        if (strcmp(find_data.cFileName, ".") == 0 || 
            strcmp(find_data.cFileName, "..") == 0 ||
            strcmp(find_data.cFileName, "temp") == 0) {
            continue;
        }

        char file_path[MAX_PATH_LENGTH];
        PathCombine(file_path, app->patient_path, find_data.cFileName);

        // Deserialize file data
        SerializedData* data = NULL;
        ErrorCode error = deserialize_from_file(file_path, &data);
        if (error != ERROR_NONE) {
            log_warning("Failed to deserialize file: %s", file_path);
            continue;
        }

        // Check if this is a CMS scan with matching filter
        if (data->data) {
            PlaybackData* playback = (PlaybackData*)data->data;
            if (playback->recording_mode == RECORDING_MODE_CMS_SCAN && 
                strcmp(playback->extra_filter, extra_filter) == 0) {
                
                // Start playback using data class
                if (app->data_class) {
                    error = data_class_start_playback(app->data_class, 
                                                    playback->data,
                                                    fast_replay,
                                                    with_summary);
                }
            }
        }

        serialized_data_destroy(data);

    } while (FindNextFile(find_handle, &find_data));

    FindClose(find_handle);
    return ERROR_NONE;
}

// Timer callback for processing command queue
/* Prehaps we will need a function similar to this in modes_sweep.c for the intial timer setup in modes_Sweep.c but
i'm not sure this function defeniiton is pretty unique. 
static void CALLBACK command_queue_timer_proc(HWND hwnd, 
                                            UINT message, 
                                            UINT_PTR timer_id, 
                                            DWORD time) {
    PlotAppReusable* app = (PlotAppReusable*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (!app) return;

    double command_data[3];  // Command + 2 parameters
    size_t data_size = sizeof(command_data);
    
    ErrorCode error = data_queue_get(app->command_queue, command_data, &data_size);
    if (error == ERROR_NONE) {
        int command = (int)command_data[0];
        switch (command) {
            case 1:  // Start command
                app->picture_windows_only = (bool)command_data[1];
                app->tilt_enabled = (bool)command_data[2];
                app->running = true;
                break;

            case 0:  // Stop command
                app->running = false;
                break;
        }
    }
}
*/

static ErrorCode process_command_queue(PlotAppReusable* app) {
    if (!app) return ERROR_INVALID_PARAMETER;

    // Check command queue (like Python's __watch_for_command__)
    double command_data[3];
    size_t data_size = sizeof(command_data);
    
    ErrorCode error = data_queue_get(app->command_queue, command_data, &data_size);
    if (error == ERROR_NONE) {
        int command = (int)command_data[0];
        if (command == 1) {  // "start"
            app->picture_windows_only = (bool)command_data[1];
            app->tilt_enabled = (bool)command_data[2];
            graph_start(app->graph, app->picture_windows_only, app->tilt_enabled);
        } else if (command == 0) {  // "stop"
            graph_stop(app->graph);
        }
    }

    // Schedule next check (like Python's after())
    Tcl_CreateTimerHandler(100, process_command_queue_callback, app);
    return error;
}

static void process_command_queue_callback(ClientData client_data) {
    PlotAppReusable* app = (PlotAppReusable*)client_data;
    process_command_queue(app);
}