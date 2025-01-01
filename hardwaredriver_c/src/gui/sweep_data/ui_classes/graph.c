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
    char* main_window_path; 
    
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
    bool cur_animation_exists;
    
    // Plot data
    double* x_data;
    size_t x_data_count;
    PlotData frontal_line;
    PlotData sagittal_line;
    
    // Canvas and figure elements
    Tk_Canvas canvas;
    int frontal_line_id;
    int sagittal_line_id;


    bool fig_exists;  // To track if figure exists (like __fig is not None)
    int figure_id;    // Canvas item ID for the main figure area
    int axis_id;      // Canvas item ID for the axis container
    
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
                   Tk_Window master,
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
    graph->main_window_path = Tk_PathName(main_window);

    // Initialize default values matching Python implementation
    graph->recording_time = 16.0;
    graph->gain_label = 45;
    graph->scan_type_label = strdup(SCAN_TYPE_VALUES[0]);
    graph->speed_label = strdup("1.0");
    graph->scan_filter_type = strdup(NONE_FILTER_VALUE);
    
    // Initialize paths and names
    graph->patient_path = strdup(patient_path);
    graph->patient_name = strdup(patient_name);
    
    // Initialize playback state
    graph->requested_playback_file_name = NULL;
    graph->temp_file_fd = NULL;
    
    // Initialize window state
    graph->picture_windows_only = false;
    graph->main_window_hidden = false;
    
    // Initialize animation state
    graph->running = false;
    InitializeCriticalSection(&graph->running_mutex);
    
    // Initialize graph state
    ///graph->show_sweep_graph = read_mode_type();
    graph->show_sweep_graph = NULL

    // Initialize plot data
    init_plot_data(&graph->frontal_line);
    init_plot_data(&graph->sagittal_line);
    graph->x_data = NULL;
    graph->x_data_count = 0;


    // Create main window and canvas (equivalent to Python's __fig creation)
    //--> we need to look into if this makes sense:
    /*    the way this works in python is that matplot lib animation and FigureCanvasTkAgg are used:
    *       _fig and ani are intialized at the top of Graph class and then __fig is passed as a paramater if animation.FuncAnimation(),
    *      this is done inside of start() function: 'self.ani = animation.FuncAnimation(self.__fig, self.__animate, None, interval=80, blit=False, cache_frame_data=False, save_count=0)'
    *      we are not using matplot lib in c, so we need to figure out how we can replicate this functionality in C,
    *      I belive we can use direct TLC/TK commands to recreate the functinality of FigureCanvasTkagg,
    *      -->the current implementation to create the main window and canvas directly in graph_create() may not be corect
    *      -->we should be able to use PlPlot lib to handle the animation functionality. 
    *      
    *      the flow for how start is actually called is:
    *      ModeSweep.__init__() 
    *     -> __sweep_command_queue.put(("start", show_tilt_window, tilt_enabled))
    *     ->PlotAppReusable.__watch_for_command__() processes command
    *     ->PlotAppReusable.start() (inherited from Graph) is called
    *    
    *
    */
    // Get the existing canvas widget (equivalent to Python's self.__canvas)
    /*
    This reference is done inside of sweep_data_reusable.c and then passed through graph_create(master)
    graph->canvas = Tk_NameToWindow(interp, ".mainwindow.graph_canvas", main_window);
    if (!graph->canvas) {
        graph_destroy(graph);
        return NULL;
    }
    */
    graph->master = master;  // Store the master reference
    graph->canvas = NULL;    // Will be set in graph_setup_graph 


    // Setup initial graph (equivalent to Python's __setup_graph__ call in __init__)
    ErrorCode error = graph_setup_graph(graph, graph->speed_label);
    if (error != ERROR_NONE) {
        graph_destroy(graph);
        return NULL;
    }

    // Setup combo values and buttons
    error = graph_setup_combo_values(graph);
    if (error != ERROR_NONE) {
        graph_destroy(graph);
        return NULL;
    }

    error = graph_setup_buttons(graph);
    if (error != ERROR_NONE) {
        graph_destroy(graph);
        return NULL;
    }

    // Create picture windows
    graph->frontal_window = picture_window_reusable_create(
        interp, patient_name, ".frontal_window", true, NULL);
    if (!graph->frontal_window) {
        graph_destroy(graph);
        return NULL;
    }

    graph->sagittal_window = picture_window_reusable_create(
        interp, patient_name, ".sagittal_window", false, NULL);
    if (!graph->sagittal_window) {
        graph_destroy(graph);
        return NULL;
    }

    if(graph->picture_windows_only){
        make_k7_window_active();`//this is from windows_api.h/c
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

    graph->show_sweep_graph = NULL;
    
    EnterCriticalSection(&graph->running_mutex);
    if (graph->running) {
        LeaveCriticalSection(&graph->running_mutex);
        return ERROR_NONE;  // Already running
    }
    LeaveCriticalSection(&graph->running_mutex);

    // Clear all data (equivalent to self.__data_class.clear_all(clear_all=True))
    data_class_clear_all(graph->data_class, true);

    graph->picture_windows_only = picture_windows_only;

    // Start picture windows if needed
    if ((picture_windows_only && tilt_enabled) || !picture_windows_only) {
        ErrorCode error = picture_window_reusable_start(graph->frontal_window);
        if (error != ERROR_NONE) return error;
        
        error = picture_window_reusable_start(graph->sagittal_window);
        if (error != ERROR_NONE) return error;
    }

    if (graph->picture_windows_only) {
        // Manual animation method - just start the timer
        // Equivalent to self.__master.after(80, self.__animate_picture_window_only)
        Tcl_CreateTimerHandler(80, (Tcl_TimerProc *)graph_animate_picture_window_only, graph);

        // Equivalent to make_k7_window_active()
        make_k7_window_active();

        if (!graph->main_window_hidden) {
            // Equivalent to self.__main_window.withdraw()
            Tcl_Eval(graph->interp, "wm withdraw .mainwindow");
            graph->main_window_hidden = true;
        }
    } else {
        if (graph->main_window_hidden) {
            // Equivalent to self.__main_window.deiconify()
            Tcl_Eval(graph->interp, "wm deiconify .mainwindow");
            graph->main_window_hidden = false;
        }

        if (!graph->cur_animation_exists) {
            // Initialize the animation
            Tcl_CreateTimerHandler(80, (Tcl_TimerProc *)graph_animate, graph);
            graph->cur_animation_exists = true;
        } else {
            // Resume case - clear exit_thread flag
            graph->namespace->exit_thread = false;
        }
    }

    graph->namespace->app_ready = true;

    EnterCriticalSection(&graph->running_mutex);
    graph->running = true;
    LeaveCriticalSection(&graph->running_mutex);

    return ERROR_NONE;
}

ErrorCode graph_stop(Graph* graph) {
    if (!graph) return ERROR_INVALID_PARAMETER;

    EnterCriticalSection(&graph->running_mutex);
    if (!graph->running) {
        LeaveCriticalSection(&graph->running_mutex);
        return ERROR_NONE;
    }

    // Set app_ready to false in namespace
    graph->namespace->app_ready = false;

    // Equivalent to ani.pause() - only set exit_thread if animation exists
    if (graph->cur_animation_exists) {
        graph->namespace->exit_thread = true;
    }

    // Stop picture windows
    if (graph->frontal_window) {
        picture_window_reusable_stop(graph->frontal_window);
    }
    if (graph->sagittal_window) {
        picture_window_reusable_stop(graph->sagittal_window);
    }

    // Hide main window if it's not already hidden
    if (!graph->main_window_hidden) {
        Tcl_Eval(graph->interp, "wm withdraw .mainwindow");
        graph->main_window_hidden = true;
    }

    graph->running = false;
    LeaveCriticalSection(&graph->running_mutex);

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

ErrorCode graph_animate(ClientData clientData) {
    Graph* graph = (Graph*)clientData;
    if (!graph) return ERROR_INVALID_PARAMETER;

    // Try to get exit_thread from namespace
    bool exit_thread;
    /*WE NEED TO IMPLEMENT GET AND SET EXIT_THREAD IN NAMESPACE_OPTIONS*/
    ErrorCode error = namespace_get_exit_thread(graph->namespace, &exit_thread);
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_EOF) {
        exit_thread = true;
    }
   

    if (exit_thread) {
        graph_stop(graph);
        return ERROR_NONE;
    }

    // Handle namespace events
    char event[MAX_EVENT_LENGTH];
    char event_data[MAX_EVENT_DATA_LENGTH];
    error = namespace_options_get_event(graph->namespace, event, MAX_EVENT_LENGTH, 
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
            data_class_toggle_recording(graph->data_class, "CMS_SCAN");

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
                data_class_save_recording(graph->data_class,
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
        } else if (namespace_event == EVENT_CMS_START_PLAYBACK) {
            if (graph->requested_playback_file_name) {
                graph->cms_callback(graph->namespace.requested_playback_file_name, 
                                true,  // fast_replay
                                true); // with_summary
            }
        } else if (namespace_event == EVENT_MARK_REDRAW_TOOL) {
            if (graph->requested_playback_file_name) {
                graph->cms_callback(graph->namespace.requested_playback_file_name,
                                true,  // fast_replay
                                false); // with_summary
            } else {
                if (graph->temp_file_fd) {
                    SweepData* saved_data = NULL;
                    sweep_data_deserialize(graph->temp_file_fd, &saved_data);
                } else {
                    // Create temp directory
                    char temp_dir[MAX_PATH];
                    snprintf(temp_dir, sizeof(temp_dir), "%s/temp", graph->patient_path);
                    CreateDirectory(temp_dir, NULL);  // Using Windows.h

                    // Generate temp filename using UUID
                    char uuid[UUID_STR_LEN];
                    uuid4(uuid);
                    
                    char temp_file[MAX_PATH];
                    snprintf(temp_file, sizeof(temp_file), "%s/%s", temp_dir, uuid);

                    // Get current points and save them
                    DataPoints* sagittal_points = data_class_get_sagittal_points(graph->data_class);
                    DataPoints* frontal_points = data_class_get_frontal_points(graph->data_class);

                    SweepData saved_data = {0};
                    saved_data.sagittal = *sagittal_points;
                    saved_data.frontal = *frontal_points;

                    sweep_data_serialize(temp_file, &saved_data);
                    graph->temp_file_fd = strdup(temp_file);
                }

                // Start playback with the saved data
                data_class_start_playback(graph->data_class, 
                                        saved_data,
                                        true,   // fast_replay
                                        false); // with_summary
            }
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
        
        double datetime_now = queue_data[2];  // Get timestamp from queue data
        
        frontal_points.values[frontal_points.count] = queue_data[0];
        frontal_points.timestamps[frontal_points.count] = datetime_now;
        frontal_points.count++;
        
        sagittal_points.values[sagittal_points.count] = queue_data[1];
        sagittal_points.timestamps[sagittal_points.count] = datetime_now;
        sagittal_points.count++;
    }

    // Append data using graph->x_data_count as points_count
    if (frontal_points.count > 0 && sagittal_points.count > 0) {
        error = data_class_append_data(graph->data_class, &frontal_points, &sagittal_points, graph->x_data_count);
    }

   /* if (sagittal_points.count > 0) {
        error = data_class_append_data(graph->data_class, sagittal_points.values, sagittal_points.count);
        free_data_points(&frontal_points);
        free_data_points(&sagittal_points);
        if (error != ERROR_NONE) return error;
    }*/

    if(graph->data_class->saved_data) {
        free_data_points(&frontal_points);
        free_data_points(&sagittal_points);
        if (error != ERROR_NONE){ return error;}
        else{ return ERROR_NONE;}
    }

     // Recording time check
    if (data_class_is_recording(graph->data_class)) {
        if (graph->show_sweep_graphc == NULL) {
            graph->show_sweep_graph = graph_getmode_type();
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

    y_points_saggital = graph->data_class->saggital_points;
    y_points_frontal = graph->data_class->frontal_points;
    graph_update_plot_data(graph, y_points_frontal, y_points_saggital);

    // Rest of the function remains the same...
    // For non-picture-windows mode, schedule next callback
    if (!graph->picture_windows_only) {
        Tcl_CreateTimerHandler(80, (Tcl_TimerProc *)graph_animate, graph);
    }
    
    return ERROR_NONE;
}

ErrorCode graph_animate_picture_window_only(ClientData clientData) {
    Graph* graph = (Graph*)clientData;
    if (!graph) return;

    // Call animate which includes the exit_thread check
    graph_animate((ClientData)graph);

    // Schedule next callback - graph_animate will have already called stop if needed
    if (!graph->namespace->exit_thread) {
        Tcl_CreateTimerHandler(80, (Tcl_TimerProc *)graph_animate_picture_window_only, graph);
    }
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

    // Use PlotData structure from graph_plot_window.h
    PlotData frontal_data = {0};
    PlotData sagittal_data = {0};

    frontal_data.x = malloc(y_points_frontal->count * sizeof(PLFLT));
    frontal_data.y = malloc(y_points_frontal->count * sizeof(PLFLT));
    sagittal_data.x = malloc(y_points_sagittal->count * sizeof(PLFLT));
    sagittal_data.y = malloc(y_points_sagittal->count * sizeof(PLFLT));

    if (!frontal_data.x || !frontal_data.y || 
        !sagittal_data.x || !sagittal_data.y) {
        free(frontal_data.x);
        free(frontal_data.y);
        free(sagittal_data.x);
        free(sagittal_data.y);
        return ERROR_OUT_OF_MEMORY;
    }

    // Scale and transform data
    double gain_scale = graph->gain_label / 15.0;
    for (size_t i = 0; i < y_points_frontal->count; i++) {
        frontal_data.x[i] = (PLFLT)i;
        sagittal_data.x[i] = (PLFLT)i;
        
        frontal_data.y[i] = (PLFLT)(y_points_frontal->values[i] / gain_scale + 180.0);
        sagittal_data.y[i] = (PLFLT)(y_points_sagittal->values[i] / gain_scale);
    }

    frontal_data.count = y_points_frontal->count;
    sagittal_data.count = y_points_sagittal->count;

    // Update plot using PLplot functions
    plcol0(1);  // blue for frontal
    plline(frontal_data.count, frontal_data.x, frontal_data.y);
    
    plcol0(2);  // red for sagittal  
    plline(sagittal_data.count, sagittal_data.x, sagittal_data.y);

    // Cleanup
    free(frontal_data.x);
    free(frontal_data.y);
    free(sagittal_data.x);
    free(sagittal_data.y);

    return ERROR_NONE;
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
    FILE* fp = fopen("C:\\K7\\current_mode_type", "r");
    if (!fp) {
        return false;
    }

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
    Tcl_CreateObjCommand(graph->interp, "C_ShowInstructions", 
                        C_ShowInstructions, (ClientData)graph, NULL);

    return ERROR_NONE;
}

// Setup buttons with registered commands
ErrorCode graph_setup_buttons(Graph* graph) {
    if (!graph) return ERROR_INVALID_PARAMETER;

    graph_register_commands(graph);
    char cmd[1024];
    
    // Create Tcl procedures that will call our C callbacks
    snprintf(cmd, sizeof(cmd),
             "proc graph_start_recording {graph_ptr} { C_StartRecording %p }; "
             "proc graph_stop_recording {graph_ptr} { C_StopRecording %p }; "
             "proc graph_save_recording {graph_ptr} { C_SaveRecording %p }; "
             "proc graph_play_recording {graph_ptr} { C_PlayRecording %p }; "
             "proc graph_pause_recording {graph_ptr} { C_PauseRecording %p }; "
             "proc graph_clear_recording {graph_ptr} { C_ClearRecording %p }; "
             "proc graph_show_plot {graph_ptr} { C_ShowPlot %p }; "
             "proc graph_show_instructions {graph_ptr} { C_ShowInstructions %p }",
             (void*)graph, (void*)graph, (void*)graph, (void*)graph,
             (void*)graph, (void*)graph, (void*)graph, (void*)graph);
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
             "-command {graph_show_plot %p} -state normal; "
             ".mainwindow.mw_fcontainer.playback_toolbar_frame.instruction_button configure "
             "-command {graph_show_instructions %p} -state normal -takefocus 0",
             (void*)graph, (void*)graph, (void*)graph, (void*)graph,
             (void*)graph, (void*)graph, (void*)graph, (void*)graph);
    
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
    
    // Trigger graph setup like Python's __set_scan_type_label
    ErrorCode error = graph_setup_graph(graph, graph->speed_label);
    if (error != ERROR_NONE) {
        return TCL_ERROR;
    }
    
    return TCL_OK;
}

static int C_SpeedChanged(ClientData clientData, Tcl_Interp* interp, 
                         int objc, Tcl_Obj* const objv[]) {
    if (objc != 2) return TCL_ERROR;
    
    Graph* graph = (Graph*)clientData;
    const char* new_value = Tcl_GetString(objv[1]);
    free(graph->speed_label);
    graph->speed_label = strdup(new_value);
    
    // Trigger graph setup like Python's __set_speed_label
    ErrorCode error = graph_setup_graph(graph, new_value);
    if (error != ERROR_NONE) {
        return TCL_ERROR;
    }
    
    return TCL_OK;
}

static int C_FilterChanged(ClientData clientData, Tcl_Interp* interp, 
                         int objc, Tcl_Obj* const objv[]) {
    if (objc != 2) return TCL_ERROR;
    
    Graph* graph = (Graph*)clientData;
    const char* new_value = Tcl_GetString(objv[1]);
    free(graph->scan_filter_type);
    graph->scan_filter_type = strdup(new_value);
    
    // Call filter table callback like Python's __set_scan_filter_type
    if (graph->filter_table_callback) {
        graph->filter_table_callback(new_value);
    }
    
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

    if (graph->fig_exists) {
        // Clear existing plot (equivalent to ax.clear())
        plsstrm(graph->figure_id);  // Switch to our figure stream
        plclear();
    } else {
        // Create new canvas as child of master (like FigureCanvasTkAgg)
        char cmd[256];
        snprintf(cmd, sizeof(cmd), 
            "canvas %s.plplot_canvas -width 600 -height 600",
            Tk_PathName(graph->master));
        if (Tcl_Eval(graph->interp, cmd) != TCL_OK) {
            return ERROR_TCL_EVAL;
        }
        
        // Get reference to new canvas
        char path[256];
        snprintf(path, sizeof(path), "%s.plplot_canvas", Tk_PathName(graph->master));
        graph->canvas = Tk_NameToWindow(graph->interp, path, graph->master);
        if (!graph->canvas) {
            return ERROR_TK_WINDOW;
        }

        // Create new stream for the plot
        graph->figure_id = plmkstrm();
        plsstrm(graph->figure_id);

        // Initialize PLPlot with tk driver
        plsdev("tk");
        plsetopt("plwindow", path);
        plsetopt("geometry", "600x600");
        plinit();
        
        // Set margins to match Python's subplots_adjust
        plsdiori(0.03, 0.05, 0.98, 0.95);

        // Grid the new canvas (equivalent to canvas.get_tk_widget().grid())
        snprintf(cmd, sizeof(cmd), 
            "grid %s.plplot_canvas -column 0 -row 2 -rowspan 20",
            Tk_PathName(graph->master));
        if (Tcl_Eval(graph->interp, cmd) != TCL_OK) {
            return ERROR_TCL_EVAL;
        }

        graph->fig_exists = true;
    }

    // Make sure we're working with our figure
    plsstrm(graph->figure_id);

    // Configure axes (equivalent to ax.grid(), ax.set_ylim, etc.)
    plcol0(15);  // Set color to black
    plenv(-90, 270, 0, 64, 0, 1);  // Set axis ranges and enable grid
    
    // Set y-axis ticks (matching Python's np.arange(-90, 271, 15))
    PLFLT yticks[25];  // (-90 to 270 by 15 = 25 ticks)
    char** ylabels = malloc(25 * sizeof(char*));
    for (int i = 0; i < 25; i++) {
        yticks[i] = -90 + (i * 15);
        ylabels[i] = strdup(" ");  // Empty labels as in Python
    }
    plsticks(NULL, ylabels, 25);  // Set y-axis ticks

    // Set x-axis ticks based on speed_label
    int x_max;
    PLFLT dx;
    if (strcmp(speed_label, "1.0") == 0) {
        x_max = 16;
        dx = 1.0;
        graph->recording_time = 16;
    } else if (strcmp(speed_label, "2.0") == 0) {
        x_max = 32;
        dx = 2.0;
        graph->recording_time = 32;
    } else {
        x_max = 64;
        dx = 4.0;
        graph->recording_time = 64;
    }

    // Create x-axis points (equivalent to np.arange(0, x_max, 0.1))
    int x_points = (int)(x_max / 0.1);
    if (graph->x_data) {
        free(graph->x_data);
    }
    graph->x_data = malloc(x_points * sizeof(double));
    graph->x_data_count = x_points;
    
    for (int i = 0; i < x_points; i++) {
        graph->x_data[i] = i * 0.1;
    }

    // Initialize plot lines with zeros (equivalent to np.zeros(len(self.X)))
    PLFLT* y_zeros = calloc(x_points, sizeof(PLFLT));
    
    // Create initial lines (equivalent to ax.plot())
    plcol0(1);  // Red for frontal
    plline(x_points, graph->x_data, y_zeros);
    graph->frontal_line_id = plgstrm();  // Store stream ID for updates
    
    plcol0(4);  // Blue for sagittal
    plline(x_points, graph->x_data, y_zeros);
    graph->sagittal_line_id = plgstrm();

    free(y_zeros);
    for (int i = 0; i < 25; i++) {
        free(ylabels[i]);
    }
    free(ylabels);

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
             "wm title .instruction_window {Sweep Mode Instructions}; "
             "label .instruction_window.text -wraplength 400; "
             "grid .instruction_window.text -row 0 -column 0; "
             "button .instruction_window.close -text Close "
             "-command {destroy .instruction_window}; "
             "grid .instruction_window.close -row 1 -column 0");
    
    if (Tcl_Eval(graph->interp, cmd) != TCL_OK) {
        return ERROR_TCL_EVAL;
    }

    // Read instructions file
    FILE* fp = fopen(INSTRUCTIONS_TEXT_FILE_PATH, "r");
    if (!fp) {
        // Handle file not found like Python version
        snprintf(cmd, sizeof(cmd),
                ".instruction_window.text configure -text {Sweep instructions not found at %s}",
                INSTRUCTIONS_TEXT_FILE_PATH);
        return (Tcl_Eval(graph->interp, cmd) == TCL_OK) ? ERROR_NONE : ERROR_TCL_EVAL;
    }

    // Read file content
    char buffer[4096] = {0};
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, fp);
    fclose(fp);

    // Set the text
    snprintf(cmd, sizeof(cmd),
             ".instruction_window.text configure -text {%s}", 
             buffer);
    
    return (Tcl_Eval(graph->interp, cmd) == TCL_OK) ? ERROR_NONE : ERROR_TCL_EVAL;
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

    // Create new graph plot window using existing implementation
    GraphPlotWindow* graph_window = graph_plot_window_create(
        graph->interp,
        graph->patient_path,
        graph->scan_filter_type,
        graph->patient_name
    );
    if (!graph_window) return ERROR_OUT_OF_MEMORY;

    // Set up window close handler first
    char close_cmd[512];
    snprintf(close_cmd, sizeof(close_cmd),
             "bind %s <Destroy> {"
             "    graph_window_close_callback %p %p"
             "}",
             graph_plot_window_get_path(graph_window),
             (void*)graph,
             (void*)graph_window);
    
    if (Tcl_Eval(graph->interp, close_cmd) != TCL_OK) {
        // Let the close callback handle cleanup
        Tcl_Eval(graph->interp, "destroy .graph_plot_window");
        return ERROR_TCL_EVAL;
    }

    // Hide existing windows using picture_window_reusable functions
    ErrorCode error = picture_window_reusable_hide_window(graph->frontal_window);
    if (error != ERROR_NONE) {
        Tcl_Eval(graph->interp, "destroy .graph_plot_window");
        return error;
    }

    error = picture_window_reusable_hide_window(graph->sagittal_window);
    if (error != ERROR_NONE) {
        Tcl_Eval(graph->interp, "destroy .graph_plot_window");
        return error;
    }

    // Hide main window
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "wm withdraw %s", graph->main_window_path);
    if (Tcl_Eval(graph->interp, cmd) != TCL_OK) {
        Tcl_Eval(graph->interp, "destroy .graph_plot_window");
        return ERROR_TCL_EVAL;
    }

    // Pause data capture
    data_class_pause_data_capture(graph->data_class);

    return ERROR_NONE;
}

// Callback function registered with Tcl
static int graph_window_close_callback(ClientData client_data, Tcl_Interp* interp, 
                              int argc, const char* argv[]) {
    if (argc != 3) return TCL_ERROR;
    
    Graph* graph = (Graph*)strtoul(argv[1], NULL, 16);
    GraphPlotWindow* graph_window = (GraphPlotWindow*)strtoul(argv[2], NULL, 16);

    // Call filter table callback
    graph->filter_table_callback(NULL);

    // Show main windows using picture_window_reusable functions
    picture_window_reusable_show_window(graph->frontal_window);
    picture_window_reusable_show_window(graph->sagittal_window);

    // Show main window
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "wm deiconify %s", graph->main_window_path);
    Tcl_Eval(interp, cmd);

    // Resume data capture
    data_class_resume_data_capture(graph->data_class);

    // Cleanup graph window - only place where we destroy the window
    graph_plot_window_destroy(graph_window);

    return TCL_OK;
}





