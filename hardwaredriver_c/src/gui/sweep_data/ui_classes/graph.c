#include "graph.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include <errno.h>
#include "logger.h"
#include "utils.h"
#include "uuid.h"

// Constants matching Python implementation
static const char* GAIN_VALUES[] = {"15", "30", "45", "90"};
static const char* SCAN_TYPE_VALUES[] = {"A/P Pitch", "Lat Roll", "Other"};
static const char* SPEED_VALUES[] = {"1.0", "2.0", "4.0"};
static const char* NONE_FILTER_VALUE = "None";
static const char* INSTRUCTIONS_TEXT_FILE_PATH = "C:\\K7\\python\\sweep_instructions.txt";
static const char* MODE_TYPE_FILE_PATH = "C:\\K7\\current_mode_type";

struct Graph {
    // TCL/TK related
    Tcl_Interp* interp;
    Tk_Window main_window;
    Tk_Window master;
    Builder* builder;
    
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
        if (strstr(buffer, "\"show_sweep_graph\": true")) {
            result = true;
        }
    }
    
    fclose(fp);
    return result;
}

static ErrorCode create_temp_data(Graph* graph) {
    if (!graph) return ERROR_INVALID_PARAMETER;
    
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
    
    char uuid[37];
    generate_uuid(uuid);
    
    char temp_file[1024];
    snprintf(temp_file, sizeof(temp_file), "%s/%s", temp_dir, uuid);
    
    const DataPoints* frontal = data_class_get_frontal_points(graph->data_class);
    const DataPoints* sagittal = data_class_get_sagittal_points(graph->data_class);
    
    FILE* fd = fopen(temp_file, "wb");
    if (!fd) return ERROR_FILE_OPERATION;
    
    SavedData saved_data = {
        .frontal_points = *frontal,
        .sagittal_points = *sagittal
    };
    
    if (fwrite(&saved_data, sizeof(SavedData), 1, fd) != 1) {
        fclose(fd);
        return ERROR_FILE_OPERATION;
    }
    
    fclose(fd);
    
    if (graph->temp_file_fd) {
        free(graph->temp_file_fd);
    }
    graph->temp_file_fd = strdup(temp_file);
    
    return ERROR_NONE;
}

static ErrorCode load_temp_data(Graph* graph) {
    if (!graph || !graph->temp_file_fd) return ERROR_INVALID_PARAMETER;
    
    FILE* fd = fopen(graph->temp_file_fd, "rb");
    if (!fd) return ERROR_FILE_OPERATION;
    
    SavedData saved_data;
    if (fread(&saved_data, sizeof(SavedData), 1, fd) != 1) {
        fclose(fd);
        return ERROR_FILE_OPERATION;
    }
    
    fclose(fd);
    
    ErrorCode err = data_class_set_points(graph->data_class,
                                        &saved_data.frontal_points,
                                        &saved_data.sagittal_points);
    
    if (err == ERROR_NONE) {
        remove(graph->temp_file_fd);
        free(graph->temp_file_fd);
        graph->temp_file_fd = NULL;
    }
    
    return err;
}

static ErrorCode save_temp_data(Graph* graph, const char* extra_filter) {
    if (!graph || !graph->temp_file_fd || !extra_filter) {
        return ERROR_INVALID_PARAMETER;
    }
    
    FILE* fd = fopen(graph->temp_file_fd, "rb");
    if (!fd) return ERROR_FILE_OPERATION;
    
    SavedData saved_data;
    if (fread(&saved_data, sizeof(SavedData), 1, fd) != 1) {
        fclose(fd);
        return ERROR_FILE_OPERATION;
    }
    
    fclose(fd);
    
    ErrorCode err = data_class_save_recording(graph->data_class,
                                            graph->patient_path,
                                            extra_filter,
                                            &saved_data);
    
    if (err == ERROR_NONE) {
        remove(graph->temp_file_fd);
        free(graph->temp_file_fd);
        graph->temp_file_fd = NULL;
    }
    
    return err;
}

ErrorCode graph_setup_combo_values(Graph* graph) {
    if (!graph) return ERROR_INVALID_PARAMETER;
    
    char cmd[1024];
    Tcl_Obj* combo_obj;
    
    // Setup gain combo
    combo_obj = builder_get_object(graph->builder, "gain_control_combo");
    if (!combo_obj) return ERROR_INVALID_STATE;
    
    snprintf(cmd, sizeof(cmd), "%s configure -values {%s %s %s %s}", 
             Tcl_GetString(combo_obj),
             GAIN_VALUES[0], GAIN_VALUES[1], GAIN_VALUES[2], GAIN_VALUES[3]);
    if (Tcl_Eval(graph->interp, cmd) != TCL_OK) return ERROR_TCL_EVAL;
    
    Tcl_CreateCommand(graph->interp, "GainComboCallback", gain_combo_callback, 
                     (ClientData)graph, NULL);
    
    snprintf(cmd, sizeof(cmd), "bind %s <<ComboboxSelected>> {GainComboCallback %%W}",
             Tcl_GetString(combo_obj));
    if (Tcl_Eval(graph->interp, cmd) != TCL_OK) return ERROR_TCL_EVAL;
    
    // Setup scan type combo
    combo_obj = builder_get_object(graph->builder, "scan_type_combo");
    if (!combo_obj) return ERROR_INVALID_STATE;
    
    snprintf(cmd, sizeof(cmd), "%s configure -values {%s %s %s}", 
             Tcl_GetString(combo_obj),
             SCAN_TYPE_VALUES[0], SCAN_TYPE_VALUES[1], SCAN_TYPE_VALUES[2]);
    if (Tcl_Eval(graph->interp, cmd) != TCL_OK) return ERROR_TCL_EVAL;
    
    Tcl_CreateCommand(graph->interp, "ScanTypeComboCallback", scan_type_combo_callback,
                     (ClientData)graph, NULL);
    
    snprintf(cmd, sizeof(cmd), "bind %s <<ComboboxSelected>> {ScanTypeComboCallback %%W}",
             Tcl_GetString(combo_obj));
    if (Tcl_Eval(graph->interp, cmd) != TCL_OK) return ERROR_TCL_EVAL;
    
    // Setup speed combo
    combo_obj = builder_get_object(graph->builder, "speed_combo");
    if (!combo_obj) return ERROR_INVALID_STATE;
    
    snprintf(cmd, sizeof(cmd), "%s configure -values {%s %s %s}", 
             Tcl_GetString(combo_obj),
             SPEED_VALUES[0], SPEED_VALUES[1], SPEED_VALUES[2]);
    if (Tcl_Eval(graph->interp, cmd) != TCL_OK) return ERROR_TCL_EVAL;
    
    Tcl_CreateCommand(graph->interp, "SpeedComboCallback", speed_combo_callback,
                     (ClientData)graph, NULL);
    
    snprintf(cmd, sizeof(cmd), "bind %s <<ComboboxSelected>> {SpeedComboCallback %%W}",
             Tcl_GetString(combo_obj));
    if (Tcl_Eval(graph->interp, cmd) != TCL_OK) return ERROR_TCL_EVAL;
    
    // Setup filter combo
    combo_obj = builder_get_object(graph->builder, "filter_combo");
    if (!combo_obj) return ERROR_INVALID_STATE;
    
    snprintf(cmd, sizeof(cmd), "%s configure -values {%s %s %s %s}", 
             Tcl_GetString(combo_obj),
             NONE_FILTER_VALUE,
             SCAN_TYPE_VALUES[0], SCAN_TYPE_VALUES[1], SCAN_TYPE_VALUES[2]);
    if (Tcl_Eval(graph->interp, cmd) != TCL_OK) return ERROR_TCL_EVAL;
    
    Tcl_CreateCommand(graph->interp, "FilterComboCallback", filter_combo_callback,
                     (ClientData)graph, NULL);
    
    snprintf(cmd, sizeof(cmd), "bind %s <<ComboboxSelected>> {FilterComboCallback %%W}",
             Tcl_GetString(combo_obj));
    if (Tcl_Eval(graph->interp, cmd) != TCL_OK) return ERROR_TCL_EVAL;
    
    return ERROR_NONE;
}

ErrorCode graph_setup_buttons(Graph* graph) {
    if (!graph) return ERROR_INVALID_PARAMETER;
    
    char cmd[1024];
    Tcl_Obj* button_obj;
    
    // Setup pause button
    button_obj = builder_get_object(graph->builder, "pause_button");
    if (!button_obj) return ERROR_INVALID_STATE;
    
    Tcl_CreateCommand(graph->interp, "PauseButtonCallback", pause_button_callback,
                     (ClientData)graph, NULL);
    
    snprintf(cmd, sizeof(cmd), "%s configure -command {PauseButtonCallback} -takefocus 0",
             Tcl_GetString(button_obj));
    if (Tcl_Eval(graph->interp, cmd) != TCL_OK) return ERROR_TCL_EVAL;
    
    // Setup play button
    button_obj = builder_get_object(graph->builder, "play_button");
    if (!button_obj) return ERROR_INVALID_STATE;
    
    Tcl_CreateCommand(graph->interp, "PlayButtonCallback", play_button_callback,
                     (ClientData)graph, NULL);
    
    snprintf(cmd, sizeof(cmd), "%s configure -command {PlayButtonCallback} -takefocus 0",
             Tcl_GetString(button_obj));
    if (Tcl_Eval(graph->interp, cmd) != TCL_OK) return ERROR_TCL_EVAL;
    
    // Setup clear button
    button_obj = builder_get_object(graph->builder, "clear_button");
    if (!button_obj) return ERROR_INVALID_STATE;
    
    Tcl_CreateCommand(graph->interp, "ClearButtonCallback", clear_button_callback,
                     (ClientData)graph, NULL);
    
    snprintf(cmd, sizeof(cmd), "%s configure -command {ClearButtonCallback} -takefocus 0",
             Tcl_GetString(button_obj));
    if (Tcl_Eval(graph->interp, cmd) != TCL_OK) return ERROR_TCL_EVAL;
    
    // Unbind space from clear button
    snprintf(cmd, sizeof(cmd), "bind %s <space> {}", Tcl_GetString(button_obj));
    if (Tcl_Eval(graph->interp, cmd) != TCL_OK) return ERROR_TCL_EVAL;
    
    // Setup start record button
    button_obj = builder_get_object(graph->builder, "start_record_button");
    if (!button_obj) return ERROR_INVALID_STATE;
    
    Tcl_CreateCommand(graph->interp, "StartRecordCallback", start_record_callback,
                     (ClientData)graph, NULL);
    
    snprintf(cmd, sizeof(cmd), "%s configure -command {StartRecordCallback} -takefocus 0",
             Tcl_GetString(button_obj));
    if (Tcl_Eval(graph->interp, cmd) != TCL_OK) return ERROR_TCL_EVAL;
    
    return ERROR_NONE;
}

Graph* graph_create(Tcl_Interp* interp, Tk_Window main_window, DataClass* data_class,
                   Builder* builder, FilterTableCallback filter_table_callback,
                   const char* patient_path, const char* patient_name,
                   CmsCallback cms_callback, NamespaceOptions* namespace) {
    Graph* graph = calloc(1, sizeof(Graph));
    if (!graph) return NULL;
    
    graph->interp = interp;
    graph->main_window = main_window;
    graph->data_class = data_class;
    graph->builder = builder;
    graph->filter_table_callback = filter_table_callback;
    graph->cms_callback = cms_callback;
    graph->namespace = namespace;
    
    // Initialize default values
    graph->recording_time = 16;
    graph->gain_label = 45;
    graph->scan_type_label = strdup(SCAN_TYPE_VALUES[0]);
    graph->speed_label = strdup("1.0");
    graph->scan_filter_type = strdup(NONE_FILTER_VALUE);
    
    if (patient_path) {
        graph->patient_path = strdup(patient_path);
    }
    if (patient_name) {
        graph->patient_name = strdup(patient_name);
    }
    
    // Initialize mutex
    InitializeCriticalSection(&graph->running_mutex);
    
    // Initialize plot data
    init_plot_data(&graph->frontal_line);
    init_plot_data(&graph->sagittal_line);
    
    // Create picture windows
    graph->frontal_window = picture_window_reusable_create(true, patient_name, 
                                                         main_window, data_class);
    graph->sagittal_window = picture_window_reusable_create(false, patient_name, 
                                                          main_window, data_class);
    
    if (!graph->frontal_window || !graph->sagittal_window) {
        graph_destroy(graph);
        return NULL;
    }
    
    picture_window_reusable_hide(graph->frontal_window);
    picture_window_reusable_hide(graph->sagittal_window);
    
    // Setup UI elements
    ErrorCode err = graph_setup_combo_values(graph);
    if (err != ERROR_NONE) {
        graph_destroy(graph);
        return NULL;
    }
    
    err = graph_setup_buttons(graph);
    if (err != ERROR_NONE) {
        graph_destroy(graph);
        return NULL;
    }
    
    err = graph_setup_graph(graph, "1.0");
    if (err != ERROR_NONE) {
        graph_destroy(graph);
        return NULL;
    }
    
    return graph;
}

void graph_destroy(Graph* graph) {
    if (!graph) return;
    
    DeleteCriticalSection(&graph->running_mutex);
    
    if (graph->frontal_window) {
        picture_window_reusable_destroy(graph->frontal_window);
    }
    if (graph->sagittal_window) {
        picture_window_reusable_destroy(graph->sagittal_window);
    }
    
    free(graph->scan_type_label);
    free(graph->speed_label);
    free(graph->scan_filter_type);
    free(graph->patient_path);
    free(graph->patient_name);
    free(graph->requested_playback_file_name);
    free(graph->temp_file_fd);
    free(graph->x_data);
    
    free_plot_data(&graph->frontal_line);
    free_plot_data(&graph->sagittal_line);
    
    free(graph);
}

ErrorCode graph_animate(Graph* graph) {
    if (!graph) return ERROR_INVALID_PARAMETER;
    
    DataClass* data_class = graph->data_class;
    NamespaceOptions* namespace = graph->namespace;
    
    // Check if we should exit
    if (namespace_get_exit_thread(namespace)) {
        graph_stop(graph);
        return ERROR_NONE;
    }
    
    // Handle namespace events
    NamespaceEvent event;
    void* event_data;
    ErrorCode err = namespace_get_event(namespace, &event, &event_data);
    if (err != ERROR_NONE) return err;
    
    if (event != EVENT_NONE) {
        switch (event) {
            case EVENT_TOGGLE_RECORDING:
                if (graph->temp_file_fd) {
                    err = load_temp_data(graph);
                    if (err != ERROR_NONE) return err;
                }
                graph->requested_playback_file_name = NULL;
                err = data_class_toggle_recording(data_class, RECORDING_MODE_CMS_SCAN);
                if (err != ERROR_NONE) return err;
                break;
                
            case EVENT_USER_RECORD_SAVED:
                if (graph->temp_file_fd) {
                    err = save_temp_data(graph, (const char*)event_data);
                    if (err != ERROR_NONE) return err;
                } else {
                    err = data_class_save_recording(data_class, 
                                                  graph->patient_path, 
                                                  (const char*)event_data,
                                                  NULL);
                    if (err != ERROR_NONE) return err;
                }
                break;
                
            case EVENT_CMS_RECORDING_PLAYBACK:
                if (!data_class_is_recording(data_class)) {
                    err = graph->cms_callback((const char*)event_data, true, false);
                    if (err != ERROR_NONE) return err;
                }
                free(graph->requested_playback_file_name);
                graph->requested_playback_file_name = strdup((const char*)event_data);
                namespace_set_requested_playback_file(namespace, (const char*)event_data);
                break;
                
            case EVENT_CMS_START_PLAYBACK:
                if (graph->requested_playback_file_name) {
                    err = graph->cms_callback(graph->requested_playback_file_name, true, true);
                    if (err != ERROR_NONE) return err;
                }
                break;
                
            case EVENT_MARK_REDRAW_TOOL:
                if (graph->requested_playback_file_name) {
                    err = graph->cms_callback(graph->requested_playback_file_name, false, true);
                    if (err != ERROR_NONE) return err;
                } else {
                    if (graph->temp_file_fd) {
                        err = load_temp_data(graph);
                        if (err != ERROR_NONE) return err;
                    } else {
                        err = create_temp_data(graph);
                        if (err != ERROR_NONE) return err;
                    }
                    
                    err = data_class_start_playback(data_class, graph->temp_file_fd, true, false);
                    if (err != ERROR_NONE) return err;
                }
                break;
        }
    }
    
    // Handle playback if active
    if (data_class_is_playback(data_class)) {
        return graph_animate_playback(graph);
    }
    
    // Handle normal recording/display
    if (!data_class_is_paused(data_class)) {
        // Process data queue
        DataQueue* queue = data_class_get_queue(data_class);
        double points[MAX_QUEUE_SIZE * 3];  // timestamp, frontal, sagittal
        size_t count = 0;
        
        while (data_queue_size(queue) > 0 && count < MAX_QUEUE_SIZE) {
            QueueData data;
            if (data_queue_get(queue, &data) != ERROR_NONE) break;
            
            if (!data_class_has_saved_data(data_class)) {
                points[count * 3] = (double)data.timestamp;
                points[count * 3 + 1] = data.frontal;
                points[count * 3 + 2] = data.sagittal;
                count++;
            }
        }
        
        if (count > 0) {
            err = data_class_append_data(data_class, points, count);
            if (err != ERROR_NONE) return err;
        }
        
        // Check recording time limit
        if (data_class_is_recording(data_class)) {
            if (graph->show_sweep_graph == -1) {
                graph->show_sweep_graph = read_mode_type();
            }
            
            time_t start_time = data_class_get_recording_start_time(data_class);
            time_t now = time(NULL);
            
            if (difftime(now, start_time) >= graph->recording_time) {
                err = data_class_stop_recording(data_class);
                if (err != ERROR_NONE) return err;
            }
        }
    }
    
    // Update plot
    const DataPoints* frontal = data_class_get_frontal_points(data_class);
    const DataPoints* sagittal = data_class_get_sagittal_points(data_class);
    
    return graph_update_plot_data(graph, frontal, sagittal);
}

ErrorCode graph_animate_playback(Graph* graph) {
    if (!graph) return ERROR_INVALID_PARAMETER;
    
    DataClass* data_class = graph->data_class;
    
    if (data_class_is_paused(data_class)) {
        return ERROR_NONE;
    }
    
    SavedData* playback_data = data_class_get_playback_data(data_class);
    if (!playback_data) return ERROR_INVALID_STATE;
    
    const DataPoints* current_frontal = data_class_get_frontal_points(data_class);
    const DataPoints* current_sagittal = data_class_get_sagittal_points(data_class);
    
    // Calculate playback speed
    double playback_speed = 100.0 * data_class_get_playback_speed(data_class);
    time_t* last_run_time = data_class_get_playback_last_run_time_ptr(data_class);
    
    if (!*last_run_time) {
        *last_run_time = playback_data->frontal_points.timestamps[0];
    }
    
    *last_run_time += (time_t)(playback_speed / 1000.0);  // Convert ms to seconds
    time_t target_time = *last_run_time;
    
    // Find data points up to current playback time
    size_t found_count = 0;
    size_t total_points = playback_data->frontal_points.count;
    size_t current_idx = 0;
    
    while (current_idx < total_points && 
           playback_data->frontal_points.timestamps[current_idx] <= target_time) {
        found_count++;
        current_idx++;
    }
    
    if (found_count > 0) {
        // Append found points to current data
        ErrorCode err = data_class_append_points(data_class,
            &(DataPoints){
                .values = playback_data->frontal_points.values,
                .timestamps = playback_data->frontal_points.timestamps,
                .count = found_count
            },
            &(DataPoints){
                .values = playback_data->sagittal_points.values,
                .timestamps = playback_data->sagittal_points.timestamps,
                .count = found_count
            });
        
        if (err != ERROR_NONE) return err;
        
        // Update image window values
        ImageWindowOptions* window_options = data_class_get_image_window_options(data_class);
        window_options->current_frontal = playback_data->frontal_points.values[found_count - 1];
        window_options->current_sagittal = playback_data->sagittal_points.values[found_count - 1];
        
        // Remove processed points from playback data
        memmove(playback_data->frontal_points.values,
                playback_data->frontal_points.values + found_count,
                (total_points - found_count) * sizeof(double));
        memmove(playback_data->frontal_points.timestamps,
                playback_data->frontal_points.timestamps + found_count,
                (total_points - found_count) * sizeof(time_t));
        
        memmove(playback_data->sagittal_points.values,
                playback_data->sagittal_points.values + found_count,
                (total_points - found_count) * sizeof(double));
        memmove(playback_data->sagittal_points.timestamps,
                playback_data->sagittal_points.timestamps + found_count,
                (total_points - found_count) * sizeof(time_t));
        
        playback_data->frontal_points.count -= found_count;
        playback_data->sagittal_points.count -= found_count;
    }
    
    // Check if playback is complete
    if (playback_data->frontal_points.count == 0) {
        ErrorCode err = data_class_stop_playback(data_class);
        if (err != ERROR_NONE) return err;
    }
    
    return graph_update_plot_data(graph, current_frontal, current_sagittal);
}

ErrorCode graph_animate_picture_window_only(Graph* graph) {
    if (!graph) return ERROR_INVALID_PARAMETER;
    
    ErrorCode err = graph_animate(graph);
    if (err != ERROR_NONE) return err;
    
    if (!namespace_get_exit_thread(graph->namespace)) {
        // Schedule next animation frame
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "after 80 {AnimatePictureWindowOnly}");
        if (Tcl_Eval(graph->interp, cmd) != TCL_OK) {
            return ERROR_TCL_EVAL;
        }
    }
    
    return ERROR_NONE;
}

ErrorCode graph_update_plot_data(Graph* graph, 
                               const DataPoints* y_points_frontal,
                               const DataPoints* y_points_sagittal) {
    if (!graph || !y_points_frontal || !y_points_sagittal) {
        return ERROR_INVALID_PARAMETER;
    }
    
    size_t points_count = y_points_frontal->count;
    double gain_factor = graph->gain_label / 15.0;
    
    // Ensure plot capacity
    ErrorCode err = ensure_plot_capacity(&graph->frontal_line, points_count);
    if (err != ERROR_NONE) return err;
    
    err = ensure_plot_capacity(&graph->sagittal_line, points_count);
    if (err != ERROR_NONE) return err;
    
    // Update plot data
    char coords[10240] = {0};  // Large buffer for coordinates
    char point_str[64];
    
    // Update sagittal line
    for (size_t i = 0; i < points_count; i++) {
        double x = graph->x_data[i];
        double y = y_points_sagittal->values[i] / gain_factor;
        snprintf(point_str, sizeof(point_str), "%.1f %.1f ", x, y);
        strcat(coords, point_str);
    }
    
    char cmd[11264];  // Larger than coords + command text
    snprintf(cmd, sizeof(cmd), ".c coords sagittal_line {%s}", coords);
    if (Tcl_Eval(graph->interp, cmd) != TCL_OK) return ERROR_TCL_EVAL;
    
    // Update frontal line (offset by 180 degrees)
    coords[0] = '\0';
    for (size_t i = 0; i < points_count; i++) {
        double x = graph->x_data[i];
        double y = (y_points_frontal->values[i] / gain_factor) + 180.0;
        snprintf(point_str, sizeof(point_str), "%.1f %.1f ", x, y);
        strcat(coords, point_str);
    }
    
    snprintf(cmd, sizeof(cmd), ".c coords frontal_line {%s}", coords);
    if (Tcl_Eval(graph->interp, cmd) != TCL_OK) return ERROR_TCL_EVAL;
    
    return ERROR_NONE;
}

ErrorCode graph_start(Graph* graph, bool picture_windows_only, bool tilt_enabled) {
    if (!graph) return ERROR_INVALID_PARAMETER;
    
    EnterCriticalSection(&graph->running_mutex);
    if (graph->running) {
        LeaveCriticalSection(&graph->running_mutex);
        return ERROR_INVALID_STATE;
    }
    
    graph->picture_windows_only = picture_windows_only;
    graph->running = true;
    LeaveCriticalSection(&graph->running_mutex);
    
    if (picture_windows_only) {
        picture_window_reusable_show(graph->frontal_window);
        picture_window_reusable_show(graph->sagittal_window);
        Tk_WithdrawWindow(graph->main_window);
        graph->main_window_hidden = true;
        
        // Start picture-only animation
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "after 80 {AnimatePictureWindowOnly}");
        if (Tcl_Eval(graph->interp, cmd) != TCL_OK) {
            return ERROR_TCL_EVAL;
        }
    } else {
        if (graph->main_window_hidden) {
            Tk_MapWindow(graph->main_window);
            graph->main_window_hidden = false;
        }
        
        if (tilt_enabled) {
            picture_window_reusable_show(graph->frontal_window);
            picture_window_reusable_show(graph->sagittal_window);
        } else {
            picture_window_reusable_hide(graph->frontal_window);
            picture_window_reusable_hide(graph->sagittal_window);
        }
        
        // Start normal animation
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "after 80 {Animate}");
        if (Tcl_Eval(graph->interp, cmd) != TCL_OK) {
            return ERROR_TCL_EVAL;
        }
    }
    
    return ERROR_NONE;
}

ErrorCode graph_stop(Graph* graph) {
    if (!graph) return ERROR_INVALID_PARAMETER;
    
    EnterCriticalSection(&graph->running_mutex);
    graph->running = false;
    LeaveCriticalSection(&graph->running_mutex);
    
    picture_window_reusable_hide(graph->frontal_window);
    picture_window_reusable_hide(graph->sagittal_window);
    
    return ERROR_NONE;
}

ErrorCode graph_update_patient_path(Graph* graph, const char* new_patient_path) {
    if (!graph || !new_patient_path) return ERROR_INVALID_PARAMETER;
    
    char* new_path = strdup(new_patient_path);
    if (!new_path) return ERROR_OUT_OF_MEMORY;
    
    free(graph->patient_path);
    graph->patient_path = new_path;
    
    return ERROR_NONE;
}

ErrorCode graph_update_patient_name(Graph* graph, const char* new_patient_name) {
    if (!graph || !new_patient_name) return ERROR_INVALID_PARAMETER;
    
    char* new_name = strdup(new_patient_name);
    if (!new_name) return ERROR_OUT_OF_MEMORY;
    
    free(graph->patient_name);
    graph->patient_name = new_name;
    
    picture_window_reusable_update_patient_name(graph->frontal_window, new_name);
    picture_window_reusable_update_patient_name(graph->sagittal_window, new_name);
    
    return ERROR_NONE;
}

// Getters
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

bool graph_is_running(const Graph* graph) {
    if (!graph) return false;
    
    bool running;
    EnterCriticalSection((CRITICAL_SECTION*)&graph->running_mutex);
    running = graph->running;
    LeaveCriticalSection((CRITICAL_SECTION*)&graph->running_mutex);
    
    return running;
}

// Tcl command registration
ErrorCode graph_register_commands(Graph* graph) {
    if (!graph) return ERROR_INVALID_PARAMETER;
    
    Tcl_CreateCommand(graph->interp, "Animate", 
                     (Tcl_CmdProc*)graph_animate,
                     (ClientData)graph, NULL);
    
    Tcl_CreateCommand(graph->interp, "AnimatePictureWindowOnly",
                     (Tcl_CmdProc*)graph_animate_picture_window_only,
                     (ClientData)graph, NULL);
    
    return ERROR_NONE;
}





