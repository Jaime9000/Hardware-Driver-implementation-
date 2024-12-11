#include "graph.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include <errno.h>
#include "logger.h"
#include "utils.h"
#include "uuid.h"
#include "cJSON.h"
#include "serialize_deserialize.h"
#include "windows_api.h"

// Constants matching Python implementation
static const char* GAIN_VALUES[] = {"15", "30", "45", "90"};
static const char* SCAN_TYPE_VALUES[] = {"A/P Pitch", "Lat Roll", "Other"};
static const char* SPEED_VALUES[] = {"1.0", "2.0", "4.0"};
static const char* NONE_FILTER_VALUE = "None";

struct Graph {
    // TCL/TK related
    Tcl_Interp* interp;
    Tk_Window main_window;
    Tk_Window master;
    
    // Data handling
    DataClass* data_class;
    FilterTableCallback filter_table_callback;
    CmsCallback cms_callback;
    NamespaceOptions* namespace;
    
    // Windows
    PictureWindowReusable* frontal_window;
    PictureWindowReusable* sagittal_window;
    bool picture_windows_only;
    bool main_window_hidden;
    
    // Graph state
    double recording_time;
    int gain_label;
    char* scan_type_label;
    char* speed_label;
    char* scan_filter_type;
    char* patient_path;
    char* patient_name;
    
    // Playback state
    char* requested_playback_file_name;
    char* temp_file_fd;
    
    // Animation state
    bool running;
    CRITICAL_SECTION running_mutex;
    bool show_sweep_graph;
    
    // Plot data
    double* x_data;
    size_t x_data_count;
    PlotData frontal_line;
    PlotData sagittal_line;
    
    // Canvas and figure elements
    Tk_Canvas canvas;
    int frontal_line_id;
    int sagittal_line_id;
    
    // Time tracking
    SYSTEMTIME playback_last_run_time;
};

static void init_plot_data(PlotData* data) {
    data->values = NULL;
    data->count = 0;
    data->capacity = 0;
}

static void free_plot_data(PlotData* data) {
    free(data->values);
    data->values = NULL;
    data->count = 0;
    data->capacity = 0;
}

static ErrorCode ensure_plot_capacity(PlotData* data, size_t needed) {
    if (needed <= data->capacity) return ERROR_NONE;
    
    size_t new_capacity = needed * 1.5;
    double* new_values = realloc(data->values, new_capacity * sizeof(double));
    
    if (!new_values) {
        return ERROR_OUT_OF_MEMORY;
    }
    
    data->values = new_values;
    data->capacity = new_capacity;
    return ERROR_NONE;
}

static bool read_mode_type(void) {
    FILE* fp = fopen(MODE_TYPE_FILE_PATH, "r");
    if (!fp) return false;
    
    char buffer[256];
    bool result = false;
    
    if (fgets(buffer, sizeof(buffer), fp)) {
        cJSON* json = cJSON_Parse(buffer);
        if (json) {
            cJSON* show_sweep = cJSON_GetObjectItem(json, "show_sweep_graph");
            if (cJSON_IsBool(show_sweep)) {
                result = cJSON_IsTrue(show_sweep);
            }
            cJSON_Delete(json);
        }
    }
    
    fclose(fp);
    return result;
}

Graph* graph_create(Tcl_Interp* interp, 
                   Tk_Window main_window,
                   DataClass* data_class,
                   FilterTableCallback filter_table_callback,
                   const char* patient_path, 
                   const char* patient_name,
                   CmsCallback cms_callback, 
                   NamespaceOptions* namespace) {
                       
    if (!interp || !data_class || !patient_path || !patient_name) return NULL;

    Graph* graph = calloc(1, sizeof(Graph));
    if (!graph) return NULL;

    // Initialize basic properties
    graph->interp = interp;
    graph->main_window = main_window;
    graph->data_class = data_class;
    graph->filter_table_callback = filter_table_callback;
    graph->cms_callback = cms_callback;
    graph->namespace = namespace;
    graph->patient_path = strdup(patient_path);
    graph->patient_name = strdup(patient_name);
    
    // Initialize default values
    graph->show_sweep_graph = false;
    graph->recording_time = 16.0;
    graph->gain_label = 45;
    graph->scan_type_label = strdup(SCAN_TYPE_VALUES[0]);
    graph->speed_label = strdup("1.0");
    graph->scan_filter_type = strdup(NONE_FILTER_VALUE);
    
    // Initialize mutex
    InitializeCriticalSection(&graph->running_mutex);
    
    // Initialize plot data
    init_plot_data(&graph->frontal_line);
    init_plot_data(&graph->sagittal_line);
    
    // Create main window and canvas
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "toplevel .graph_window; "
             "wm title .graph_window {Graph}; "
             "canvas .graph_window.canvas -width 700 -height 400; "
             "pack .graph_window.canvas -expand yes -fill both");
    
    if (Tcl_Eval(interp, cmd) != TCL_OK) {
        graph_destroy(graph);
        return NULL;
    }
    
    // Store canvas widget
    graph->canvas = Tk_NameToWindow(interp, ".graph_window.canvas", main_window);
    if (!graph->canvas) {
        graph_destroy(graph);
        return NULL;
    }

    // Setup combo values and buttons
    if (graph_setup_combo_values(graph) != ERROR_NONE ||
        graph_setup_buttons(graph) != ERROR_NONE ||
        graph_setup_graph(graph, graph->speed_label) != ERROR_NONE) {
        graph_destroy(graph);
        return NULL;
    }

    return graph;
}

void graph_destroy(Graph* graph) {
    if (!graph) return;

    EnterCriticalSection(&graph->running_mutex);
    graph->running = false;
    LeaveCriticalSection(&graph->running_mutex);

    // Destroy windows
    if (graph->frontal_window) {
        picture_window_reusable_destroy(graph->frontal_window);
    }
    if (graph->sagittal_window) {
        picture_window_reusable_destroy(graph->sagittal_window);
    }

    // Free plot data
    free_plot_data(&graph->frontal_line);
    free_plot_data(&graph->sagittal_line);
    free(graph->x_data);

    // Free strings
    free(graph->patient_path);
    free(graph->patient_name);
    free(graph->scan_type_label);
    free(graph->speed_label);
    free(graph->scan_filter_type);
    free(graph->requested_playback_file_name);
    free(graph->temp_file_fd);

    // Destroy Tcl/Tk elements
    if (graph->interp) {
        Tcl_Eval(graph->interp, "destroy .graph_window");
    }

    DeleteCriticalSection(&graph->running_mutex);
    free(graph);
}

ErrorCode graph_start(Graph* graph, bool picture_windows_only, bool tilt_enabled) {
    if (!graph) return ERROR_INVALID_PARAMETER;

    graph->picture_windows_only = picture_windows_only;
    
    // Create picture windows if needed
    if (!graph->frontal_window) {
        graph->frontal_window = picture_window_reusable_create(
            graph->interp, graph->patient_name, ".graph_window", true, NULL);
        if (!graph->frontal_window) return ERROR_WINDOW_CREATE;
    }
    
    if (!graph->sagittal_window) {
        graph->sagittal_window = picture_window_reusable_create(
            graph->interp, graph->patient_name, ".graph_window", false, NULL);
        if (!graph->sagittal_window) return ERROR_WINDOW_CREATE;
    }

    // Start picture windows
    ErrorCode error = picture_window_reusable_start(graph->frontal_window);
    if (error != ERROR_NONE) return error;
    
    error = picture_window_reusable_start(graph->sagittal_window);
    if (error != ERROR_NONE) return error;

    EnterCriticalSection(&graph->running_mutex);
    graph->running = true;
    LeaveCriticalSection(&graph->running_mutex);

    return ERROR_NONE;
}

ErrorCode graph_stop(Graph* graph) {
    if (!graph) return ERROR_INVALID_PARAMETER;

    EnterCriticalSection(&graph->running_mutex);
    graph->running = false;
    LeaveCriticalSection(&graph->running_mutex);

    if (graph->frontal_window) {
        picture_window_reusable_stop(graph->frontal_window);
    }
    if (graph->sagittal_window) {
        picture_window_reusable_stop(graph->sagittal_window);
    }

    return ERROR_NONE;
}

bool graph_is_running(const Graph* graph) {
    if (!graph) return false;
    
    bool running;
    EnterCriticalSection((CRITICAL_SECTION*)&graph->running_mutex);
    running = graph->running;
    LeaveCriticalSection((CRITICAL_SECTION*)&graph->running_mutex);
    
    return running;
}

ErrorCode graph_animate(Graph* graph) {
    if (!graph) return ERROR_INVALID_PARAMETER;
    
    // Check if we should stop animation
    if (!graph_is_running(graph)) {
        return ERROR_NONE;
    }

    // Check namespace exit_thread from struct directly since there's no getter
    if (graph->namespace->exit_thread) {
        graph_stop(graph);
        return ERROR_NONE;
    }

    // Handle namespace events
    char event[MAX_EVENT_LENGTH];
    char event_data[MAX_EVENT_DATA_LENGTH];
    ErrorCode error = namespace_options_get_event(graph->namespace, event, MAX_EVENT_LENGTH, 
                                                event_data, MAX_EVENT_DATA_LENGTH);
    if (error != ERROR_NONE) return error;
    
    if (event[0] != '\0') {  // Event exists
        if (strcmp(event, EVENT_TOGGLE_RECORDING) == 0) {
            // Load temp data if exists
            if (graph->temp_file_fd) {
                error = graph_load_temp_data(graph);
                if (error != ERROR_NONE) return error;
                graph->temp_file_fd = NULL;
            }
            graph->requested_playback_file_name = NULL;
            return data_class_toggle_recording(graph->data_class, "CMS_SCAN");

        } else if (strcmp(event, EVENT_USER_RECORD_SAVED) == 0) {
            if (graph->temp_file_fd) {
                // Handle saving with temp data
                SavedData saved_data;
                error = load_saved_data(graph->temp_file_fd, &saved_data);
                if (error != ERROR_NONE) return error;
                error = data_class_save_recording(graph->data_class, 
                                                graph->patient_path,
                                                event_data,
                                                &saved_data,
                                                "CMS_SCAN");
                graph->temp_file_fd = NULL;
                return error;
            } else {
                return data_class_save_recording(graph->data_class,
                                               graph->patient_path,
                                               event_data,
                                               NULL,
                                               NULL);
            }

        } else if (strcmp(event, EVENT_CMS_RECORDING_PLAYBACK) == 0) {
            if (!data_class_is_recording(graph->data_class)) {
                graph->cms_callback(event_data, false, true);
            }
            free(graph->requested_playback_file_name);
            graph->requested_playback_file_name = strdup(event_data);
            
            // Store in namespace struct directly since there's no setter
            strncpy(graph->namespace->requested_playback_file, event_data, MAX_PATH_LENGTH - 1);
        }
    }

    // Handle playback if active
    if (data_class_is_playback(graph->data_class)) {
        return graph_animate_playback(graph);
    }

    // Handle recording pause
    if (data_class_is_paused(graph->data_class)) {
        return ERROR_NONE;
    }

    // Recording time check
    if (data_class_is_recording(graph->data_class)) {
        if (!graph->show_sweep_graph) {
            graph->show_sweep_graph = read_mode_type();
        }
        if (graph->show_sweep_graph) {
            SYSTEMTIME current_time;
            GetLocalTime(&current_time);
            double elapsed_time = get_time_difference_seconds(&graph->data_class->recording_start_time, &current_time);
            if (elapsed_time >= graph->recording_time) {
                data_class_stop_recording(graph->data_class);
            }
        }
    }

    // Process data queue
    DataQueue* data_queue = graph->data_class->data_queue;
    DataPoints frontal_points = {0};
    DataPoints sagittal_points = {0};
    
    // Initialize both points arrays with initial capacity
    error = ensure_capacity(&frontal_points, 100);
    if (error != ERROR_NONE) return error;
    
    error = ensure_capacity(&sagittal_points, 100);
    if (error != ERROR_NONE) {
        free_data_points(&frontal_points);
        return error;
    }
    
    while (data_queue_size(data_queue) > 0) {
        double queue_data[3];  // [frontal, sagittal, datetime]
        size_t count = 3;
        
        error = data_queue_get(data_queue, queue_data, &count);
        if (error == ERROR_QUEUE_EMPTY || count != 3) break;
        
        // Skip if we have saved data
        const SavedData* saved_data = data_class_get_saved_data(graph->data_class);
        if (saved_data != NULL) continue;
        
        // Add points to both frontal and sagittal
        error = ensure_capacity(&frontal_points, frontal_points.count + 1);
        if (error != ERROR_NONE) {
            free_data_points(&frontal_points);
            free_data_points(&sagittal_points);
            return error;
        }
        
        error = ensure_capacity(&sagittal_points, sagittal_points.count + 1);
        if (error != ERROR_NONE) {
            free_data_points(&frontal_points);
            free_data_points(&sagittal_points);
            return error;
        }
        
        frontal_points.values[frontal_points.count] = queue_data[0];     // frontal value
        frontal_points.timestamps[frontal_points.count] = queue_data[2]; // datetime
        frontal_points.count++;
        
        sagittal_points.values[sagittal_points.count] = queue_data[1];     // sagittal value
        sagittal_points.timestamps[sagittal_points.count] = queue_data[2]; // datetime
        sagittal_points.count++;
    }

    // Update data class with new points if we have any
    if (frontal_points.count > 0) {
        error = data_class_append_data(graph->data_class, frontal_points.values, frontal_points.count);
        if (error != ERROR_NONE) {
            free_data_points(&frontal_points);
            free_data_points(&sagittal_points);
            return error;
        }
    }

    if (sagittal_points.count > 0) {
        error = data_class_append_data(graph->data_class, sagittal_points.values, sagittal_points.count);
        free_data_points(&frontal_points);
        free_data_points(&sagittal_points);
        if (error != ERROR_NONE) return error;
    }

    // Rest of the function remains the same...
    return ERROR_NONE;
}

ErrorCode graph_animate_picture_window_only(Graph* graph) {
    if (!graph) return ERROR_INVALID_PARAMETER;

    ErrorCode error = graph_animate(graph);
    if (error != ERROR_NONE) return error;

    if (graph_is_running(graph)) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), 
                "after 80 {graph_animate_picture_window_only %p}", (void*)graph);
        if (Tcl_Eval(graph->interp, cmd) != TCL_OK) {
            return ERROR_TCL_EVAL;
        }
    }

    return ERROR_NONE;
}

ErrorCode graph_animate_playback(Graph* graph) {
    if (!graph || !graph->data_class) return ERROR_INVALID_PARAMETER;

    if (data_class_is_paused(graph->data_class)) {
        return ERROR_NONE;
    }

    const SavedData* playback_data = data_class_get_saved_data(graph->data_class);
    if (!playback_data) return ERROR_NO_DATA;

    const DataPoints* frontal_points = data_class_get_frontal_points(graph->data_class);
    const DataPoints* sagittal_points = data_class_get_sagittal_points(graph->data_class);

    double playback_speed = data_class_get_playback_speed(graph->data_class) * 100;
    
    // Update playback time
    SYSTEMTIME current_time;
    GetLocalTime(&current_time);
    
    if (graph->playback_last_run_time.wYear == 0) {  // First run
        graph->playback_last_run_time = current_time;
    } else {
        // Add playback_speed milliseconds to last run time
        FILETIME ft;
        SystemTimeToFileTime(&graph->playback_last_run_time, &ft);
        ULARGE_INTEGER uli;
        uli.LowPart = ft.dwLowDateTime;
        uli.HighPart = ft.dwHighDateTime;
        uli.QuadPart += (ULONGLONG)(playback_speed * 10000);  // Convert to 100-nanosecond intervals
        ft.dwLowDateTime = uli.LowPart;
        ft.dwHighDateTime = uli.HighPart;
        FileTimeToSystemTime(&ft, &graph->playback_last_run_time);
    }

    // Update plot data
    ErrorCode error = graph_update_plot_data(graph, frontal_points, sagittal_points);
    if (error != ERROR_NONE) return error;

    if (graph_is_running(graph)) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), 
                "after 80 {graph_animate_playback %p}", (void*)graph);
        if (Tcl_Eval(graph->interp, cmd) != TCL_OK) {
            return ERROR_TCL_EVAL;
        }
    }

    return ERROR_NONE;
}

ErrorCode graph_update_patient_name(Graph* graph, const char* new_patient_name) {
    if (!graph || !new_patient_name) return ERROR_INVALID_PARAMETER;

    free(graph->patient_name);
    graph->patient_name = strdup(new_patient_name);
    if (!graph->patient_name) return ERROR_OUT_OF_MEMORY;

    // Update picture windows
    if (graph->frontal_window) {
        ErrorCode error = picture_window_reusable_update_patient_name(
            graph->frontal_window, new_patient_name);
        if (error != ERROR_NONE) return error;
    }
    
    if (graph->sagittal_window) {
        ErrorCode error = picture_window_reusable_update_patient_name(
            graph->sagittal_window, new_patient_name);
        if (error != ERROR_NONE) return error;
    }

    return ERROR_NONE;
}

ErrorCode graph_update_patient_path(Graph* graph, const char* new_patient_path) {
    if (!graph || !new_patient_path) return ERROR_INVALID_PARAMETER;

    free(graph->patient_path);
    graph->patient_path = strdup(new_patient_path);
    if (!graph->patient_path) return ERROR_OUT_OF_MEMORY;

    return ERROR_NONE;
}

ErrorCode graph_update_plot_data(Graph* graph, 
                               const DataPoints* y_points_frontal, 
                               const DataPoints* y_points_sagittal) {
    if (!graph || !y_points_frontal || !y_points_sagittal) {
        return ERROR_INVALID_PARAMETER;
    }

    // Ensure capacity for plot data
    ErrorCode error = ensure_plot_capacity(&graph->frontal_line, y_points_frontal->count);
    if (error != ERROR_NONE) return error;

    error = ensure_plot_capacity(&graph->sagittal_line, y_points_sagittal->count);
    if (error != ERROR_NONE) return error;

    // Update x_data if needed
    if (graph->x_data_count < y_points_frontal->count) {
        double* new_x_data = realloc(graph->x_data, 
                                   y_points_frontal->count * sizeof(double));
        if (!new_x_data) return ERROR_OUT_OF_MEMORY;
        
        graph->x_data = new_x_data;
        for (size_t i = graph->x_data_count; i < y_points_frontal->count; i++) {
            graph->x_data[i] = (double)i;
        }
        graph->x_data_count = y_points_frontal->count;
    }

    // Copy data points
    memcpy(graph->frontal_line.values, y_points_frontal->values, 
           y_points_frontal->count * sizeof(double));
    graph->frontal_line.count = y_points_frontal->count;

    memcpy(graph->sagittal_line.values, y_points_sagittal->values, 
           y_points_sagittal->count * sizeof(double));
    graph->sagittal_line.count = y_points_sagittal->count;

    // Update plot
    return graph_plot_graph_values(graph);
}

// Property getters
double graph_get_gain_label(const Graph* graph) {
    return graph ? graph->gain_label : 45.0;  // Default value if NULL
}

const char* graph_get_scan_type_label(const Graph* graph) {
    return graph ? graph->scan_type_label : SCAN_TYPE_VALUES[0];
}

double graph_get_speed_label(const Graph* graph) {
    return graph ? atof(graph->speed_label) : 1.0;
}

const char* graph_get_scan_filter_type(const Graph* graph) {
    return graph ? graph->scan_filter_type : NONE_FILTER_VALUE;
}

bool graph_get_mode_type(void) {
    return read_mode_type();
}

// Tcl command callbacks that will be registered
static int C_StartRecording(ClientData clientData, Tcl_Interp* interp, 
                          int objc, Tcl_Obj* const objv[]) {
    Graph* graph = (Graph*)clientData;
    
    ErrorCode error = data_class_start_recording(graph->data_class, graph->scan_type_label);
    if (error == ERROR_NONE) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd),
                ".mainwindow.mw_fcontainer.recording_toolbar_frame.start_record_button configure -state disabled; "
                ".mainwindow.mw_fcontainer.recording_toolbar_frame.stop_record_button configure -state normal; "
                ".mainwindow.mw_fcontainer.recording_toolbar_frame.save_record_button configure -state normal");
        Tcl_Eval(graph->interp, cmd);
    }
    return TCL_OK;
}

static int C_StopRecording(ClientData clientData, Tcl_Interp* interp, 
                         int objc, Tcl_Obj* const objv[]) {
    Graph* graph = (Graph*)clientData;
    
    ErrorCode error = data_class_stop_recording(graph->data_class);
    if (error == ERROR_NONE) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd),
                ".mainwindow.mw_fcontainer.recording_toolbar_frame.start_record_button configure -state normal; "
                ".mainwindow.mw_fcontainer.recording_toolbar_frame.stop_record_button configure -state disabled; "
                ".mainwindow.mw_fcontainer.playback_toolbar_frame.play_button configure -state normal");
        Tcl_Eval(graph->interp, cmd);
    }
    return TCL_OK;
}

static int C_SaveRecording(ClientData clientData, Tcl_Interp* interp, 
                         int objc, Tcl_Obj* const objv[]) {
    Graph* graph = (Graph*)clientData;
    
    ErrorCode error = data_class_save_recording(graph->data_class);
    if (error == ERROR_NONE) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd),
                ".mainwindow.mw_fcontainer.recording_toolbar_frame.save_record_button configure -state disabled");
        Tcl_Eval(graph->interp, cmd);
    }
    return TCL_OK;
}

static int C_PlayRecording(ClientData clientData, Tcl_Interp* interp, 
                         int objc, Tcl_Obj* const objv[]) {
    Graph* graph = (Graph*)clientData;
    
    ErrorCode error = data_class_start_playback(graph->data_class);
    if (error == ERROR_NONE) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd),
                ".mainwindow.mw_fcontainer.playback_toolbar_frame.play_button configure -state disabled; "
                ".mainwindow.mw_fcontainer.playback_toolbar_frame.pause_button configure -state normal");
        Tcl_Eval(graph->interp, cmd);
    }
    return TCL_OK;
}

static int C_PauseRecording(ClientData clientData, Tcl_Interp* interp, 
                          int objc, Tcl_Obj* const objv[]) {
    Graph* graph = (Graph*)clientData;
    
    ErrorCode error = data_class_pause_playback(graph->data_class);
    if (error == ERROR_NONE) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd),
                ".mainwindow.mw_fcontainer.playback_toolbar_frame.play_button configure -state normal; "
                ".mainwindow.mw_fcontainer.playback_toolbar_frame.pause_button configure -state disabled");
        Tcl_Eval(graph->interp, cmd);
    }
    return TCL_OK;
}

static int C_ClearRecording(ClientData clientData, Tcl_Interp* interp, 
                          int objc, Tcl_Obj* const objv[]) {
    Graph* graph = (Graph*)clientData;
    
    ErrorCode error = data_class_clear_data(graph->data_class);
    if (error == ERROR_NONE) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd),
                ".mainwindow.mw_fcontainer.recording_toolbar_frame.save_record_button configure -state disabled; "
                ".mainwindow.mw_fcontainer.playback_toolbar_frame.play_button configure -state disabled; "
                ".mainwindow.mw_fcontainer.playback_toolbar_frame.pause_button configure -state disabled");
        Tcl_Eval(graph->interp, cmd);
    }
    return TCL_OK;
}

static int C_ShowPlot(ClientData clientData, Tcl_Interp* interp, 
                     int objc, Tcl_Obj* const objv[]) {
    Graph* graph = (Graph*)clientData;
    return graph_plot_window_show(graph->plot_window) == ERROR_NONE ? TCL_OK : TCL_ERROR;
}

static int C_CloseInstructions(ClientData clientData, Tcl_Interp* interp, 
                              int objc, Tcl_Obj* const objv[]) {
    Graph* graph = (Graph*)clientData;
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "destroy .instruction_window");
    Tcl_Eval(graph->interp, cmd);
    return TCL_OK;
}

// Register all callbacks with Tcl
ErrorCode graph_register_commands(Graph* graph) {
    if (!graph) return ERROR_INVALID_PARAMETER;

    Tcl_CreateObjCommand(graph->interp, "C_StartRecording", C_StartRecording, (ClientData)graph, NULL);
    Tcl_CreateObjCommand(graph->interp, "C_StopRecording", C_StopRecording, (ClientData)graph, NULL);
    Tcl_CreateObjCommand(graph->interp, "C_SaveRecording", C_SaveRecording, (ClientData)graph, NULL);
    Tcl_CreateObjCommand(graph->interp, "C_PlayRecording", C_PlayRecording, (ClientData)graph, NULL);
    Tcl_CreateObjCommand(graph->interp, "C_PauseRecording", C_PauseRecording, (ClientData)graph, NULL);
    Tcl_CreateObjCommand(graph->interp, "C_ClearRecording", C_ClearRecording, (ClientData)graph, NULL);
    Tcl_CreateObjCommand(graph->interp, "C_ShowPlot", C_ShowPlot, (ClientData)graph, NULL);
    Tcl_CreateObjCommand(graph->interp, "C_CloseInstructions", 
                        C_CloseInstructions, (ClientData)graph, NULL);

    return ERROR_NONE;
}

// Setup buttons with registered commands
ErrorCode graph_setup_buttons(Graph* graph) {
    if (!graph) return ERROR_INVALID_PARAMETER;

    char cmd[1024];
    
    // Create Tcl procedures that will call our C callbacks
    snprintf(cmd, sizeof(cmd),
             "proc graph_start_recording {graph_ptr} { C_StartRecording %p }; "
             "proc graph_stop_recording {graph_ptr} { C_StopRecording %p }; "
             "proc graph_save_recording {graph_ptr} { C_SaveRecording %p }; "
             "proc graph_play_recording {graph_ptr} { C_PlayRecording %p }; "
             "proc graph_pause_recording {graph_ptr} { C_PauseRecording %p }; "
             "proc graph_clear_recording {graph_ptr} { C_ClearRecording %p }; "
             "proc graph_show_plot {graph_ptr} { C_ShowPlot %p }",
             (void*)graph, (void*)graph, (void*)graph, (void*)graph,
             (void*)graph, (void*)graph, (void*)graph);
    if (Tcl_Eval(graph->interp, cmd) != TCL_OK) {
        return ERROR_TCL_EVAL;
    }

    // Configure all buttons with their commands and initial states
    snprintf(cmd, sizeof(cmd),
             ".mainwindow.mw_fcontainer.recording_toolbar_frame.start_record_button configure "
             "-command {graph_start_recording %p} -state normal; "
             ".mainwindow.mw_fcontainer.recording_toolbar_frame.stop_record_button configure "
             "-command {graph_stop_recording %p} -state disabled; "
             ".mainwindow.mw_fcontainer.recording_toolbar_frame.save_record_button configure "
             "-command {graph_save_recording %p} -state disabled; "
             ".mainwindow.mw_fcontainer.playback_toolbar_frame.play_button configure "
             "-command {graph_play_recording %p} -state disabled; "
             ".mainwindow.mw_fcontainer.playback_toolbar_frame.pause_button configure "
             "-command {graph_pause_recording %p} -state disabled; "
             ".mainwindow.mw_fcontainer.playback_toolbar_frame.clear_button configure "
             "-command {graph_clear_recording %p} -state normal; "
             ".mainwindow.mw_fcontainer.mw_fbottom.graph_button configure "
             "-command {graph_show_plot %p} -state normal",
             (void*)graph, (void*)graph, (void*)graph, (void*)graph,
             (void*)graph, (void*)graph, (void*)graph);
    
    if (Tcl_Eval(graph->interp, cmd) != TCL_OK) {
        return ERROR_TCL_EVAL;
    }

    // Add global key binding if not picture windows only
    if (!graph->picture_windows_only) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd),
                "bind . <KeyRelease> {"
                "    graph_handle_key_release %p %%K"  // %K gives us the key symbol
                "}", (void*)graph);
        if (Tcl_Eval(graph->interp, cmd) != TCL_OK) {
            return ERROR_TCL_EVAL;
        }

        // Register the key handler procedure
        snprintf(cmd, sizeof(cmd),
                "proc graph_handle_key_release {graph_ptr key} {"
                "    C_HandleKeyRelease %p $key"
                "}", (void*)graph);
        if (Tcl_Eval(graph->interp, cmd) != TCL_OK) {
            return ERROR_TCL_EVAL;
        }
    }

    return ERROR_NONE;
}

// Helper functions to get indices (similar to Python's .index())
static int get_gain_index(int gain_label) {
    char gain_str[8];
    snprintf(gain_str, sizeof(gain_str), "%d", gain_label);
    for (int i = 0; i < GAIN_VALUES_COUNT; i++) {
        if (strcmp(GAIN_VALUES[i], gain_str) == 0) {
            return i;
        }
    }
    return 0;  // Default to first value if not found
}

static int get_scan_type_index(const char* scan_type_label) {
    for (int i = 0; i < SCAN_TYPE_VALUES_COUNT; i++) {
        if (strcmp(SCAN_TYPE_VALUES[i], scan_type_label) == 0) {
            return i;
        }
    }
    return 0;
}

static int get_speed_index(const char* speed_label) {
    for (int i = 0; i < SPEED_VALUES_COUNT; i++) {
        if (strcmp(SPEED_VALUES[i], speed_label) == 0) {
            return i;
        }
    }
    return 0;
}

static int get_filter_index(const char* filter_type) {
    for (int i = 0; i < FILTER_COMBO_VALUES_COUNT; i++) {
        if (strcmp(FILTER_COMBO_VALUES[i], filter_type) == 0) {
            return i;
        }
    }
    return 0;
}

// Callback functions for combo box selections
static int C_GainChanged(ClientData clientData, Tcl_Interp* interp, 
                        int objc, Tcl_Obj* const objv[]) {
    if (objc != 2) return TCL_ERROR;
    
    Graph* graph = (Graph*)clientData;
    const char* new_value = Tcl_GetString(objv[1]);
    graph->gain_label = atoi(new_value);  // Convert string to int for gain
    
    return TCL_OK;
}

static int C_ScanTypeChanged(ClientData clientData, Tcl_Interp* interp, 
                           int objc, Tcl_Obj* const objv[]) {
    if (objc != 2) return TCL_ERROR;
    
    Graph* graph = (Graph*)clientData;
    const char* new_value = Tcl_GetString(objv[1]);
    free(graph->scan_type_label);
    graph->scan_type_label = strdup(new_value);
    
    return TCL_OK;
}

static int C_SpeedChanged(ClientData clientData, Tcl_Interp* interp, 
                         int objc, Tcl_Obj* const objv[]) {
    if (objc != 2) return TCL_ERROR;
    
    Graph* graph = (Graph*)clientData;
    const char* new_value = Tcl_GetString(objv[1]);
    free(graph->speed_label);
    graph->speed_label = strdup(new_value);
    
    return TCL_OK;
}

static int C_FilterChanged(ClientData clientData, Tcl_Interp* interp, 
                         int objc, Tcl_Obj* const objv[]) {
    if (objc != 2) return TCL_ERROR;
    
    Graph* graph = (Graph*)clientData;
    const char* new_value = Tcl_GetString(objv[1]);
    free(graph->scan_filter_type);
    graph->scan_filter_type = strdup(new_value);
    
    return TCL_OK;
}

ErrorCode graph_setup_combo_values(Graph* graph) {
    if (!graph) return ERROR_INVALID_PARAMETER;

    char cmd[1024];
    
    // Register C callbacks
    Tcl_CreateObjCommand(graph->interp, "C_GainChanged", C_GainChanged, (ClientData)graph, NULL);
    Tcl_CreateObjCommand(graph->interp, "C_ScanTypeChanged", C_ScanTypeChanged, (ClientData)graph, NULL);
    Tcl_CreateObjCommand(graph->interp, "C_SpeedChanged", C_SpeedChanged, (ClientData)graph, NULL);
    Tcl_CreateObjCommand(graph->interp, "C_FilterChanged", C_FilterChanged, (ClientData)graph, NULL);

    // Create Tcl procedures for callbacks
    snprintf(cmd, sizeof(cmd),
             "proc graph_gain_changed {graph_ptr value} { C_GainChanged %p $value }; "
             "proc graph_scan_type_changed {graph_ptr value} { C_ScanTypeChanged %p $value }; "
             "proc graph_speed_changed {graph_ptr value} { C_SpeedChanged %p $value }; "
             "proc graph_filter_changed {graph_ptr value} { C_FilterChanged %p $value }",
             (void*)graph, (void*)graph, (void*)graph, (void*)graph);
    if (Tcl_Eval(graph->interp, cmd) != TCL_OK) {
        return ERROR_TCL_EVAL;
    }

    // Configure gain combo
    snprintf(cmd, sizeof(cmd),
             ".mainwindow.mw_fcontainer.mw_fbottom.gain_control_combo configure "
             "-values {%s %s %s %s} -state readonly; "
             ".mainwindow.mw_fcontainer.mw_fbottom.gain_control_combo current %d; "
             "bind .mainwindow.mw_fcontainer.mw_fbottom.gain_control_combo <<ComboboxSelected>> {"
             "    graph_gain_changed %p [.mainwindow.mw_fcontainer.mw_fbottom.gain_control_combo get]"
             "}",
             GAIN_VALUES[0], GAIN_VALUES[1], GAIN_VALUES[2], GAIN_VALUES[3],
             get_gain_index(graph->gain_label),
             (void*)graph);
    if (Tcl_Eval(graph->interp, cmd) != TCL_OK) {
        return ERROR_TCL_EVAL;
    }

    // Configure scan type combo
    snprintf(cmd, sizeof(cmd),
             ".mainwindow.mw_fcontainer.mw_fbottom.scan_type_combo configure "
             "-values {%s %s %s} -state readonly; "
             ".mainwindow.mw_fcontainer.mw_fbottom.scan_type_combo current %d; "
             "bind .mainwindow.mw_fcontainer.mw_fbottom.scan_type_combo <<ComboboxSelected>> {"
             "    graph_scan_type_changed %p [.mainwindow.mw_fcontainer.mw_fbottom.scan_type_combo get]"
             "}",
             SCAN_TYPE_VALUES[0], SCAN_TYPE_VALUES[1], SCAN_TYPE_VALUES[2],
             get_scan_type_index(graph->scan_type_label),
             (void*)graph);
    if (Tcl_Eval(graph->interp, cmd) != TCL_OK) {
        return ERROR_TCL_EVAL;
    }

    // Configure speed combo
    snprintf(cmd, sizeof(cmd),
             ".mainwindow.mw_fcontainer.mw_fbottom.speed_combo configure "
             "-values {%s %s %s} -state readonly; "
             ".mainwindow.mw_fcontainer.mw_fbottom.speed_combo current %d; "
             "bind .mainwindow.mw_fcontainer.mw_fbottom.speed_combo <<ComboboxSelected>> {"
             "    graph_speed_changed %p [.mainwindow.mw_fcontainer.mw_fbottom.speed_combo get]"
             "}",
             SPEED_VALUES[0], SPEED_VALUES[1], SPEED_VALUES[2],
             get_speed_index(graph->speed_label),
             (void*)graph);
    if (Tcl_Eval(graph->interp, cmd) != TCL_OK) {
        return ERROR_TCL_EVAL;
    }

    // Configure filter combo
    snprintf(cmd, sizeof(cmd),
             ".mainwindow.mw_fcontainer.mw_fbottom.filter_combo configure "
             "-values {%s %s %s %s} -state readonly; "
             ".mainwindow.mw_fcontainer.mw_fbottom.filter_combo current %d; "
             "bind .mainwindow.mw_fcontainer.mw_fbottom.filter_combo <<ComboboxSelected>> {"
             "    graph_filter_changed %p [.mainwindow.mw_fcontainer.mw_fbottom.filter_combo get]"
             "}",
             FILTER_COMBO_VALUES[0], FILTER_COMBO_VALUES[1], 
             FILTER_COMBO_VALUES[2], FILTER_COMBO_VALUES[3],
             get_filter_index(graph->scan_filter_type),
             (void*)graph);
    if (Tcl_Eval(graph->interp, cmd) != TCL_OK) {
        return ERROR_TCL_EVAL;
    }

    return ERROR_NONE;
}

ErrorCode graph_setup_graph(Graph* graph, const char* speed_label) {
    if (!graph || !speed_label) return ERROR_INVALID_PARAMETER;

    // Configure canvas for plotting
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             ".mainwindow.mw_fcontainer.graph_canvas configure -width 700 -height 400; "
             ".mainwindow.mw_fcontainer.graph_canvas delete all");
    if (Tcl_Eval(graph->interp, cmd) != TCL_OK) {
        return ERROR_TCL_EVAL;
    }

    // Create initial plot lines
    graph->frontal_line_id = Tk_CreateLine(graph->canvas, NULL, 0, 0, 0, 0,
                                         "fill", "blue", "width", "2", NULL);
    graph->sagittal_line_id = Tk_CreateLine(graph->canvas, NULL, 0, 0, 0, 0,
                                          "fill", "red", "width", "2", NULL);

    if (!graph->frontal_line_id || !graph->sagittal_line_id) {
        return ERROR_TCL_EVAL;
    }

    return ERROR_NONE;
}

ErrorCode graph_handle_key_press(Graph* graph, int keycode) {
    if (!graph) return ERROR_INVALID_PARAMETER;

    // Handle spacebar (ASCII 32)
    if (keycode == 32) {
        if (data_class_is_recording(graph->data_class)) {
            return data_class_stop_recording(graph->data_class);
        } else {
            return data_class_toggle_recording(graph->data_class, graph->scan_type_label);
        }
    }

    return ERROR_NONE;
}

ErrorCode graph_show_instructions(Graph* graph) {
    if (!graph) return ERROR_INVALID_PARAMETER;

    // Create instructions window
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "toplevel .instruction_window; "
             "wm title .instruction_window {Instructions}; "
             "text .instruction_window.text -width 60 -height 20 "
             "-yscrollcommand {.instruction_window.scroll set}; "
             "scrollbar .instruction_window.scroll -command {.instruction_window.text yview}; "
             "pack .instruction_window.scroll -side right -fill y; "
             "pack .instruction_window.text -side left -fill both -expand 1");
    if (Tcl_Eval(graph->interp, cmd) != TCL_OK) {
        return ERROR_TCL_EVAL;
    }

    // Read and display instructions from file
    FILE* fp = fopen(INSTRUCTIONS_TEXT_FILE_PATH, "r");
    if (!fp) {
        return ERROR_FILE_OPEN;
    }

    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        snprintf(cmd, sizeof(cmd),
                ".instruction_window.text insert end {%s}", line);
        if (Tcl_Eval(graph->interp, cmd) != TCL_OK) {
            fclose(fp);
            return ERROR_TCL_EVAL;
        }
    }

    fclose(fp);
    return ERROR_NONE;
}

ErrorCode graph_create_temp_data(Graph* graph) {
    if (!graph) return ERROR_INVALID_PARAMETER;
    
    // Create temp directory if it doesn't exist
    char temp_dir[512];
    snprintf(temp_dir, sizeof(temp_dir), "%s/temp", graph->patient_path);
    
    #ifdef _WIN32
    if (_mkdir(temp_dir) != 0 && errno != EEXIST) {
        return ERROR_FILE_OPERATION;
    }
    #else
    if (mkdir(temp_dir, 0777) != 0 && errno != EEXIST) {
        return ERROR_FILE_OPERATION;
    }
    #endif
    
    // Generate UUID for temp file
    char uuid[37];
    generate_uuid(uuid);
    
    char temp_file[1024];
    snprintf(temp_file, sizeof(temp_file), "%s/%s", temp_dir, uuid);
    
    // Get current data points
    const DataPoints* frontal = data_class_get_frontal_points(graph->data_class);
    const DataPoints* sagittal = data_class_get_sagittal_points(graph->data_class);
    
    if (!frontal || !sagittal) {
        return ERROR_INVALID_STATE;
    }

    // Create SavedData structure
    SavedData saved_data = {0};
    saved_data.frontal_points = *frontal;
    saved_data.sagittal_points = *sagittal;
    saved_data.scan_type = graph->scan_type_label ? strdup(graph->scan_type_label) : NULL;
    saved_data.recording_time = graph->recording_time;
    saved_data.gain_label = graph->gain_label;
    saved_data.speed_label = graph->speed_label ? strdup(graph->speed_label) : NULL;

    // Serialize data to temp file
    ErrorCode error = serialize_to_file(temp_file, &saved_data, sizeof(SavedData));
    
    // Free allocated strings
    free(saved_data.scan_type);
    free(saved_data.speed_label);
    
    if (error != ERROR_NONE) {
        return error;
    }

    // Store temp file path
    free(graph->temp_file_fd);
    graph->temp_file_fd = strdup(temp_file);
    
    return ERROR_NONE;
}

ErrorCode graph_load_temp_data(Graph* graph) {
    if (!graph || !graph->temp_file_fd) return ERROR_INVALID_PARAMETER;

    SavedData saved_data = {0};
    
    // Deserialize data from temp file
    ErrorCode error = deserialize_from_file(graph->temp_file_fd, &saved_data, sizeof(SavedData));
    if (error != ERROR_NONE) {
        return error;
    }

    // Update graph properties
    if (saved_data.scan_type) {
        free(graph->scan_type_label);
        graph->scan_type_label = strdup(saved_data.scan_type);
    }
    
    if (saved_data.speed_label) {
        free(graph->speed_label);
        graph->speed_label = strdup(saved_data.speed_label);
    }
    
    graph->recording_time = saved_data.recording_time;
    graph->gain_label = saved_data.gain_label;

    // Update data points
    error = graph_update_plot_data(graph, 
                                 &saved_data.frontal_points, 
                                 &saved_data.sagittal_points);
    
    // Free allocated memory
    free(saved_data.scan_type);
    free(saved_data.speed_label);
    
    return error;
}

ErrorCode graph_plot_graph_values(Graph* graph) {
    if (!graph) return ERROR_INVALID_PARAMETER;

    // Clear previous plot
    char cmd[256];
    snprintf(cmd, sizeof(cmd), ".graph_window.canvas delete all");
    if (Tcl_Eval(graph->interp, cmd) != TCL_OK) {
        return ERROR_TCL_EVAL;
    }

    // Calculate plot dimensions
    int canvas_width = 700;  // From setup_graph
    int canvas_height = 400;
    int margin = 50;
    int plot_width = canvas_width - 2 * margin;
    int plot_height = canvas_height - 2 * margin;

    // Find data range
    MinMaxValues frontal_range = {0};
    MinMaxValues sagittal_range = {0};
    
    calculate_min_max_values(graph->frontal_line.values, 
                           graph->frontal_line.count, 
                           &frontal_range);
    calculate_min_max_values(graph->sagittal_line.values, 
                           graph->sagittal_line.count, 
                           &sagittal_range);

    double y_min = fmin(frontal_range.min_value, sagittal_range.min_value);
    double y_max = fmax(frontal_range.max_value, sagittal_range.max_value);
    double y_range = y_max - y_min;

    // Create coordinate arrays for both lines
    int max_points = (int)fmax(graph->frontal_line.count, graph->sagittal_line.count);
    XPoint* frontal_coords = malloc(sizeof(XPoint) * graph->frontal_line.count);
    XPoint* sagittal_coords = malloc(sizeof(XPoint) * graph->sagittal_line.count);
    
    if (!frontal_coords || !sagittal_coords) {
        free(frontal_coords);
        free(sagittal_coords);
        return ERROR_OUT_OF_MEMORY;
    }

    // Calculate coordinates for frontal line
    for (size_t i = 0; i < graph->frontal_line.count; i++) {
        frontal_coords[i].x = margin + (int)(i * plot_width / (max_points - 1));
        frontal_coords[i].y = canvas_height - margin - 
            (int)((graph->frontal_line.values[i] - y_min) * plot_height / y_range);
    }

    // Calculate coordinates for sagittal line
    for (size_t i = 0; i < graph->sagittal_line.count; i++) {
        sagittal_coords[i].x = margin + (int)(i * plot_width / (max_points - 1));
        sagittal_coords[i].y = canvas_height - margin - 
            (int)((graph->sagittal_line.values[i] - y_min) * plot_height / y_range);
    }

    // Draw lines
    if (graph->frontal_line.count > 1) {
        Tk_DrawLines(graph->canvas, frontal_coords, graph->frontal_line.count,
                    graph->frontal_line_id);
    }
    
    if (graph->sagittal_line.count > 1) {
        Tk_DrawLines(graph->canvas, sagittal_coords, graph->sagittal_line.count,
                    graph->sagittal_line_id);
    }

    // Draw axes
    snprintf(cmd, sizeof(cmd),
             ".mainwindow.mw_fcontainer.graph_canvas create line %d %d %d %d -width 1; "  // X-axis
             ".mainwindow.mw_fcontainer.graph_canvas create line %d %d %d %d -width 1",   // Y-axis
             margin, canvas_height - margin,
             canvas_width - margin, canvas_height - margin,
             margin, margin,
             margin, canvas_height - margin);
    if (Tcl_Eval(graph->interp, cmd) != TCL_OK) {
        free(frontal_coords);
        free(sagittal_coords);
        return ERROR_TCL_EVAL;
    }

    // Add labels
    snprintf(cmd, sizeof(cmd),
             ".mainwindow.mw_fcontainer.graph_canvas create text %d %d -text {Time} -anchor n; "
             ".mainwindow.mw_fcontainer.graph_canvas create text %d %d -text {Value} -anchor s -angle 90",
             canvas_width / 2, canvas_height - 10,
             10, canvas_height / 2);
    if (Tcl_Eval(graph->interp, cmd) != TCL_OK) {
        free(frontal_coords);
        free(sagittal_coords);
        return ERROR_TCL_EVAL;
    }

    free(frontal_coords);
    free(sagittal_coords);
    return ERROR_NONE;
}





