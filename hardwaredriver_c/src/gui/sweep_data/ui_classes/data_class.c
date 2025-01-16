#include "data_class.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "logger.h"
#include "../utils.h"
#include <stdio.h>
#include "cJSON.h"
#include <windows.h>

#define DEFAULT_PLAYBACK_SPEED 2.0
#define DEFAULT_CONFIG_PATH "C:\\K7\\playback_speeds"

// Add string arrays for enums
const char* RECORDING_MODE_STRINGS[] = {
    "A/P Pitch",
    "Lat Roll",
    "Other",
    "CMS_SCAN"
};

const char* RECORDING_STATE_STRINGS[] = {
    "Recording",
    "Recording Complete",
    "",  // NOT_RECORDING
    "Playing",
    "Playback Complete",
    "Playback Paused",
    "Error: Not recording data yet"
};

// Implement calculate_min_max_values as a public function
ErrorCode calculate_min_max_values(const double* values, 
                                 size_t count, 
                                 MinMaxValues* result) {
    if (!values || !result || count == 0) return ERROR_INVALID_PARAMETER;
    
    double max_value = values[0];
    double min_value = values[0];
    
    for (size_t i = 1; i < count; i++) {
        if (values[i] > max_value) max_value = values[i];
        if (values[i] < min_value) min_value = values[i];
    }
    
    if ((max_value > 0 && min_value > 0) || (max_value < 0 && min_value < 0)) {
        if (max_value > 0) {
            max_value = max_value - min_value;
            min_value = 0;
        } else {
            min_value = min_value - max_value;
            max_value = 0;
        }
    }
    
    result->max_value = max_value;
    result->min_value = min_value;
    return ERROR_NONE;
}

struct DataClass {
    Tcl_Interp* interp;
    char* builder_path;
    char* namespace_path;
    DataQueue* data_queue;
    
    // Data points
    DataPoints frontal_points;
    DataPoints sagittal_points;
    SavedData* saved_data;
    SavedData** saved_data_points;
    size_t saved_data_count;
    size_t saved_data_capacity;
    
    // Recording state
    RecordingState state;
    bool recording_on;
    bool recording_paused;
    SYSTEMTIME recording_start_time;
    char* recording_scan_type;
    
    // Playback state
    bool playback_on;
    SavedData* playback_data;
    SYSTEMTIME playback_last_run_time;
    double playback_speed;
    bool fast_replay;
    
    // UI elements
    ImageWindowOptions* window_options;
    char* status_label_path;
    char* play_button_path;
    char* pause_button_path;
    char* start_record_button_path;
    char* stop_record_button_path;
    char* save_record_button_path;
};

static void init_data_points(DataPoints* points) {
    points->values = NULL;
    points->timestamps = NULL;
    points->count = 0;
    points->capacity = 0;
}

static void free_data_points(DataPoints* points) {
    free(points->values);
    free(points->timestamps);
    init_data_points(points);
}

static ErrorCode ensure_capacity(DataPoints* points, size_t needed) {
    if (needed <= points->capacity) return ERROR_NONE;
    
    size_t new_capacity = needed * 1.5;
    double* new_values = realloc(points->values, new_capacity * sizeof(double));
    double* new_timestamps = realloc(points->timestamps, new_capacity * sizeof(double));
    
    if (!new_values || !new_timestamps) {
        free(new_values);
        free(new_timestamps);
        return ERROR_OUT_OF_MEMORY;
    }
    
    points->values = new_values;
    points->timestamps = new_timestamps;
    points->capacity = new_capacity;
    return ERROR_NONE;
}

static ErrorCode sanitize_points(const DataPoints* input, DataPoints* output) {
    if (!input || !output) return ERROR_INVALID_PARAMETER;
    
    output->count = 0;
    if (input->count == 0) return ERROR_NONE;
    
    ErrorCode error = ensure_capacity(output, input->count);
    if (error != ERROR_NONE) return error;
    
    for (size_t i = 0; i < input->count; i++) {
        if (!isnan(input->values[i])) {
            output->values[output->count] = input->values[i];
            output->timestamps[output->count] = input->timestamps[i];
            output->count++;
        }
    }
    
    return ERROR_NONE;
}

static ErrorCode compute_rolling_average(DataClass* data, size_t points_count) {
    if (!data || points_count == 0) return ERROR_INVALID_PARAMETER;
    
    // Initialize temporary storage
    DataPoints temp_frontal = {0};
    DataPoints temp_sagittal = {0};
    init_data_points(&temp_frontal);
    init_data_points(&temp_sagittal);
    
    // Allocate space for averaged points
    ErrorCode error = ensure_capacity(&temp_frontal, points_count);
    if (error != ERROR_NONE) return error;
    
    error = ensure_capacity(&temp_sagittal, points_count);
    if (error != ERROR_NONE) {
        free_data_points(&temp_frontal);
        return error;
    }
    
    // Get current time relative to recording start
    SYSTEMTIME current_time;
    GetLocalTime(&current_time);
    double current_seconds = get_time_difference_seconds(&data->recording_start_time, 
                                                       &current_time);
    
    const double INTERVAL = 0.1;  // 0.1 second intervals
    
    // Calculate averages for each interval
    for (size_t i = 0; i < points_count; i++) {
        double interval_end = current_seconds - (i * INTERVAL);
        double interval_start = interval_end - INTERVAL;
        
        double frontal_sum = 0.0;
        double sagittal_sum = 0.0;
        size_t frontal_count = 0;
        size_t sagittal_count = 0;
        
        // Calculate frontal average
        for (size_t j = data->frontal_points.count; j > 0; j--) {
            double point_time = data->frontal_points.timestamps[j-1];
            if (point_time <= interval_start) break;
            if (point_time > interval_end) continue;
            
            frontal_sum += data->frontal_points.values[j-1];
            frontal_count++;
        }
        
        // Calculate sagittal average
        for (size_t j = data->sagittal_points.count; j > 0; j--) {
            double point_time = data->sagittal_points.timestamps[j-1];
            if (point_time <= interval_start) break;
            if (point_time > interval_end) continue;
            
            sagittal_sum += data->sagittal_points.values[j-1];
            sagittal_count++;
        }
        
        // Store results (newest points at end)
        size_t idx = points_count - 1 - i;
        temp_frontal.timestamps[idx] = interval_end;
        temp_sagittal.timestamps[idx] = interval_end;
        
        // Store averages or use previous values
        if (frontal_count > 0) {
            temp_frontal.values[idx] = frontal_sum / frontal_count;
        } else if (i > 0) {
            temp_frontal.values[idx] = temp_frontal.values[idx + 1];
        } else if (data->frontal_points.count > 0) {
            temp_frontal.values[idx] = data->frontal_points.values[data->frontal_points.count - 1];
        }
        
        if (sagittal_count > 0) {
            temp_sagittal.values[idx] = sagittal_sum / sagittal_count;
        } else if (i > 0) {
            temp_sagittal.values[idx] = temp_sagittal.values[idx + 1];
        } else if (data->sagittal_points.count > 0) {
            temp_sagittal.values[idx] = data->sagittal_points.values[data->sagittal_points.count - 1];
        }
        
        temp_frontal.count++;
        temp_sagittal.count++;
    }
    
    // Update main data structures
    error = ensure_capacity(&data->frontal_points, points_count);
    if (error != ERROR_NONE) {
        free_data_points(&temp_frontal);
        free_data_points(&temp_sagittal);
        return error;
    }
    
    error = ensure_capacity(&data->sagittal_points, points_count);
    if (error != ERROR_NONE) {
        free_data_points(&temp_frontal);
        free_data_points(&temp_sagittal);
        return error;
    }
    
    // Copy averaged data
    memcpy(data->frontal_points.values, temp_frontal.values, points_count * sizeof(double));
    memcpy(data->frontal_points.timestamps, temp_frontal.timestamps, points_count * sizeof(double));
    memcpy(data->sagittal_points.values, temp_sagittal.values, points_count * sizeof(double));
    memcpy(data->sagittal_points.timestamps, temp_sagittal.timestamps, points_count * sizeof(double));
    
    data->frontal_points.count = points_count;
    data->sagittal_points.count = points_count;
    
    // Cleanup
    free_data_points(&temp_frontal);
    free_data_points(&temp_sagittal);
    
    return ERROR_NONE;
}

static void setup_ui_paths(DataClass* data) {
    char path[256];
    
    snprintf(path, sizeof(path), "%s.frame2.current_status", data->builder_path);
    data->status_label_path = strdup(path);
    
    snprintf(path, sizeof(path), "%s.playback_toolbar_frame.play_button", data->builder_path);
    data->play_button_path = strdup(path);
    
    snprintf(path, sizeof(path), "%s.playback_toolbar_frame.pause_button", data->builder_path);
    data->pause_button_path = strdup(path);
    
    snprintf(path, sizeof(path), "%s.recording_toolbar_frame.start_record_button", data->builder_path);
    data->start_record_button_path = strdup(path);
    
    snprintf(path, sizeof(path), "%s.recording_toolbar_frame.stop_record_button", data->builder_path);
    data->stop_record_button_path = strdup(path);
    
    snprintf(path, sizeof(path), "%s.recording_toolbar_frame.save_record_button", data->builder_path);
    data->save_record_button_path = strdup(path);
}

static void update_recording_label(DataClass* data) {
    if (!data) return;
    
    char cmd[256];
    const char* state_text;
    
    switch (data->state) {
        case RECORDING_ON:
            if (data->recording_scan_type) {
                snprintf(cmd, sizeof(cmd), "Recording: %s", data->recording_scan_type);
                state_text = cmd;
            } else {
                state_text = "Recording";
            }
            break;
        case RECORDING_COMPLETE:
            state_text = "Recording Complete";
            break;
        case PLAYBACK:
            state_text = "Playing";
            break;
        case PLAYBACK_COMPLETE:
            state_text = "Playback Complete";
            break;
        case PLAYBACK_PAUSED:
            state_text = "Playback Paused";
            break;
        case ERROR_NOT_RECORDING:
            state_text = "Error: Not recording data yet";
            break;
        default:
            state_text = "";
    }
    
    snprintf(cmd, sizeof(cmd), "%s configure -text {%s}", 
             data->status_label_path, state_text);
    Tcl_Eval(data->interp, cmd);
}

DataClass* data_class_create(Tcl_Interp* interp, 
                           const char* builder_path,
                           const char* namespace_path,
                           DataQueue* data_queue) {
    DataClass* data = calloc(1, sizeof(DataClass));
    if (!data) return NULL;
    
    data->interp = interp;
    data->builder_path = strdup(builder_path);
    data->namespace_path = strdup(namespace_path);
    data->data_queue = data_queue;
    
    // Initialize data points
    init_data_points(&data->frontal_points);
    init_data_points(&data->sagittal_points);
    
    // Initialize saved data array
    data->saved_data_capacity = 10;
    data->saved_data_points = malloc(sizeof(SavedData*) * data->saved_data_capacity);
    if (!data->saved_data_points) {
        data_class_destroy(data);
        return NULL;
    }
    
    // Initialize window options
    data->window_options = calloc(1, sizeof(ImageWindowOptions));
    if (!data->window_options) {
        data_class_destroy(data);
        return NULL;
    }
    
    // Setup UI paths and initial state
    setup_ui_paths(data);
    data->state = NOT_RECORDING;
    data->playback_speed = 1.0;
    
    update_recording_label(data);
    
    return data;
}

void data_class_destroy(DataClass* data) {
    if (!data) return;
    
    // Free data points
    free_data_points(&data->frontal_points);
    free_data_points(&data->sagittal_points);
    
    // Free saved data
    if (data->saved_data) {
        free_data_points(&data->saved_data->frontal_points);
        free_data_points(&data->saved_data->sagittal_points);
        free(data->saved_data->scan_type);
        free(data->saved_data);
    }
    
    // Free saved data points array
    for (size_t i = 0; i < data->saved_data_count; i++) {
        if (data->saved_data_points[i]) {
            free_data_points(&data->saved_data_points[i]->frontal_points);
            free_data_points(&data->saved_data_points[i]->sagittal_points);
            free(data->saved_data_points[i]->scan_type);
            free(data->saved_data_points[i]);
        }
    }
    free(data->saved_data_points);
    
    // Free playback data
    if (data->playback_data) {
        free_data_points(&data->playback_data->frontal_points);
        free_data_points(&data->playback_data->sagittal_points);
        free(data->playback_data->scan_type);
        free(data->playback_data);
    }
    
    // Free strings
    free(data->builder_path);
    free(data->namespace_path);
    free(data->recording_scan_type);
    free(data->status_label_path);
    free(data->play_button_path);
    free(data->pause_button_path);
    free(data->start_record_button_path);
    free(data->stop_record_button_path);
    free(data->save_record_button_path);
    
    // Free window options
    free(data->window_options);
    
    // Free the struct itself
    free(data);
}

ErrorCode data_class_start_recording(DataClass* data, const char* scan_type_label) {
    if (!data || !scan_type_label) return ERROR_INVALID_PARAMETER;
    
    data_class_clear_all(data, !data->recording_paused);
    
    data->recording_on = true;
    data->recording_paused = false;
    data->state = RECORDING_ON;
    GetLocalTime(&data->recording_start_time);
    
    // Set scan type
    free(data->recording_scan_type);
    data->recording_scan_type = strdup(scan_type_label);
    
    // Update UI
    update_recording_label(data);
    
    // Set button states
    data_class_set_button_state(data, data->start_record_button_path, false);
    data_class_set_button_state(data, data->stop_record_button_path, true);
    data_class_set_button_state(data, data->save_record_button_path, false);
    
    return ERROR_NONE;
}

ErrorCode data_class_stop_recording(DataClass* data) {
    if (!data) return ERROR_INVALID_PARAMETER;
    
    if (!data->recording_on) {
        data->state = ERROR_NOT_RECORDING;
        update_recording_label(data);
        return ERROR_INVALID_STATE;
    }
    
    data->recording_on = false;
    data->recording_paused = false;
    data->state = RECORDING_COMPLETE;
    
    // Update UI
    update_recording_label(data);
    data_class_set_button_state(data, data->start_record_button_path, true);
    data_class_set_button_state(data, data->stop_record_button_path, false);
    data_class_set_button_state(data, data->save_record_button_path, true);
    
    return ERROR_NONE;
}

ErrorCode data_class_toggle_recording(DataClass* data, const char* scan_type_label) {
    if (!data) return ERROR_INVALID_PARAMETER;
    
    if (data->recording_on && !data->recording_paused) {
        return data_class_stop_recording(data);
    } else {
        return data_class_start_recording(data, scan_type_label);
    }
}

ErrorCode data_class_pause_data_capture(DataClass* data) {
    if (!data) return ERROR_INVALID_PARAMETER;
    
    if (!data->recording_on) return ERROR_INVALID_STATE;
    
    data->recording_paused = true;
    return ERROR_NONE;
}

ErrorCode data_class_resume_data_capture(DataClass* data) {
    if (!data) return ERROR_INVALID_PARAMETER;
    
    if (!data->recording_on) return ERROR_INVALID_STATE;
    
    data->recording_paused = false;
    return ERROR_NONE;
}

ErrorCode data_class_clear_all(DataClass* data, bool clear_all) {
    if (!data) return ERROR_INVALID_PARAMETER;
    
    data->recording_paused = false;
    data->playback_on = false;
    
    if (clear_all) {
        data_class_reinitialize_y_points(data);
        image_window_options_reset(data->window_options);
    }
    
    data->state = NOT_RECORDING;
    update_recording_label(data);
    
    // Clear playback data
    if (data->playback_data) {
        free_data_points(&data->playback_data->frontal_points);
        free_data_points(&data->playback_data->sagittal_points);
        free(data->playback_data->scan_type);
        free(data->playback_data);
        data->playback_data = NULL;
    }
    data->playback_last_run_time = 0;
    
    // Clear saved data
    if (data->saved_data) {
        free_data_points(&data->saved_data->frontal_points);
        free_data_points(&data->saved_data->sagittal_points);
        free(data->saved_data->scan_type);
        free(data->saved_data);
        data->saved_data = NULL;
    }
    
    // Clear queue
    data_queue_clear(data->data_queue);
    
    // Reset recording state
    data->recording_on = false;
    data->recording_start_time = 0;
    free(data->recording_scan_type);
    data->recording_scan_type = NULL;
    
    // Update UI buttons
    data_class_set_button_state(data, data->play_button_path, false);
    data_class_set_button_state(data, data->pause_button_path, false);
    data_class_set_button_state(data, data->start_record_button_path, true);
    data_class_set_button_state(data, data->stop_record_button_path, false);
    data_class_set_button_state(data, data->save_record_button_path, false);
    
    return ERROR_NONE;
}

ErrorCode data_class_append_data(DataClass* data, 
                               const double* y_points, 
                               size_t points_count) {
    if (!data || !y_points || points_count == 0) return ERROR_INVALID_PARAMETER;
    
    // Get time since recording started
    SYSTEMTIME current_time;
    GetLocalTime(&current_time);
    double seconds_from_start = get_time_difference_seconds(&data->recording_start_time, 
                                                          &current_time);
    
    // Ensure we have space for new points
    ErrorCode error = ensure_capacity(&data->frontal_points, 
                                    data->frontal_points.count + points_count);
    if (error != ERROR_NONE) return error;
    
    error = ensure_capacity(&data->sagittal_points, 
                          data->sagittal_points.count + points_count);
    if (error != ERROR_NONE) return error;
    
    // Store the new points
    for (size_t i = 0; i < points_count; i++) {
        data->frontal_points.values[data->frontal_points.count] = y_points[i * 2];
        data->frontal_points.timestamps[data->frontal_points.count] = seconds_from_start;
        data->frontal_points.count++;
        
        data->sagittal_points.values[data->sagittal_points.count] = y_points[i * 2 + 1];
        data->sagittal_points.timestamps[data->sagittal_points.count] = seconds_from_start;
        data->sagittal_points.count++;
    }
    
    return compute_rolling_average(data, points_count);
}

ErrorCode data_class_save_recording(DataClass* data, 
                                  const char* patient_path, 
                                  const char* extra_filter) {
    if (!data || !patient_path) return ERROR_INVALID_PARAMETER;
    
    // Stop recording if needed
    if (data->recording_on && !data->recording_paused) {
        ErrorCode error = data_class_stop_recording(data);
        if (error != ERROR_NONE) return error;
    } else if (!data->recording_on) {
        data->state = ERROR_NOT_RECORDING;
        update_recording_label(data);
        return ERROR_INVALID_STATE;
    }

    // Get encoded datetime for filename using utils.h function
    char datetime_str[MAX_DATETIME_LENGTH];
    ErrorCode error = encode_curr_datetime(datetime_str, sizeof(datetime_str));
    if (error != ERROR_NONE) return error;

    // Create full path
    char full_path[MAX_PATH];
    snprintf(full_path, sizeof(full_path), "%s\\%s", patient_path, datetime_str);

    // Save the data
    error = sweep_data_serialize(full_path, data->saved_data, extra_filter);
    if (error != ERROR_NONE) return error;

    data->recording_on = false;
    data->recording_paused = false;

    return ERROR_NONE;
}

ErrorCode data_class_set_image_window_values(DataClass* data, 
                                           double frontal, 
                                           double sagittal) {
    if (!data || !data->window_options) return ERROR_INVALID_PARAMETER;
    
    data->window_options->current_frontal = frontal;
    data->window_options->current_sagittal = sagittal;
    
    // Update min/max values
    if (frontal > data->window_options->max_frontal) {
        data->window_options->max_frontal = frontal;
    }
    if (frontal < data->window_options->min_frontal) {
        data->window_options->min_frontal = frontal;
    }
    
    if (sagittal > data->window_options->max_sagittal) {
        data->window_options->max_sagittal = sagittal;
    }
    if (sagittal < data->window_options->min_sagittal) {
        data->window_options->min_sagittal = sagittal;
    }
    
    return ERROR_NONE;
}

ErrorCode data_class_reinitialize_y_points(DataClass* data) {
    if (!data) return ERROR_INVALID_PARAMETER;
    
    data->frontal_points.count = 0;
    data->sagittal_points.count = 0;
    
    return ERROR_NONE;
}

ErrorCode data_class_load_playback_speed(const char* config_path, double* speed) {
    if (!speed) return ERROR_INVALID_PARAMETER;
    
    // Try to read the file
    FILE* f = fopen(config_path, "r");
    if (!f) {
        // If file doesn't exist or can't be read, use default
        *speed = DEFAULT_PLAYBACK_SPEED;
        return data_class_save_playback_speed(config_path, *speed);
    }
    
    // Get file size
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    // Read file content
    char* content = malloc(fsize + 1);
    if (!content) {
        fclose(f);
        return ERROR_OUT_OF_MEMORY;
    }
    
    fread(content, 1, fsize, f);
    content[fsize] = 0;
    fclose(f);
    
    // Parse JSON
    cJSON* root = cJSON_Parse(content);
    free(content);
    
    if (!root) {
        // If parsing fails, use default
        *speed = DEFAULT_PLAYBACK_SPEED;
        return data_class_save_playback_speed(config_path, *speed);
    }
    
    // Get speed value
    if (!cJSON_IsNumber(root)) {
        cJSON_Delete(root);
        *speed = DEFAULT_PLAYBACK_SPEED;
        return data_class_save_playback_speed(config_path, *speed);
    }
    
    *speed = root->valuedouble;
    cJSON_Delete(root);
    
    return ERROR_NONE;
}

ErrorCode data_class_save_playback_speed(const char* config_path, double speed) {
    cJSON* root = cJSON_CreateNumber(speed);
    if (!root) return ERROR_OUT_OF_MEMORY;
    
    char* json_str = cJSON_Print(root);
    cJSON_Delete(root);
    
    if (!json_str) return ERROR_OUT_OF_MEMORY;
    
    FILE* f = fopen(config_path, "w");
    if (!f) {
        free(json_str);
        return ERROR_FILE_OPERATION;
    }
    
    fprintf(f, "%s", json_str);
    free(json_str);
    fclose(f);
    
    return ERROR_NONE;
}

ErrorCode data_class_start_playback(DataClass* data, 
                                  const char* filename, 
                                  bool fast_replay,
                                  bool with_summary) {
    if (!data || !filename) return ERROR_INVALID_PARAMETER;
    
    // Stop any current recording/playback
    data_class_clear_all(data, true);
    
    // Load playback data
    SavedData* playback_data = NULL;
    ErrorCode error = sweep_data_deserialize(filename, &playback_data);
    if (error != ERROR_NONE) return error;
    
    data->playback_data = playback_data;
    data->playback_on = true;
    data->state = PLAYBACK;
    data->fast_replay = fast_replay;
    
    // Handle playback speed based on fast_replay
    if (fast_replay) {
        error = data_class_load_playback_speed(DEFAULT_CONFIG_PATH, &data->playback_speed);
        if (error != ERROR_NONE) {
            // If loading fails, use default speed
            data->playback_speed = DEFAULT_PLAYBACK_SPEED;
            data_class_save_playback_speed(DEFAULT_CONFIG_PATH, DEFAULT_PLAYBACK_SPEED);
        }
    } else {
        data->playback_speed = 1.0;
    }
    
    // Handle with_summary parameter
    if (with_summary && playback_data && 
        playback_data->frontal_points.count > 0) {
        data->playback_last_run_time = 
            playback_data->frontal_points.timestamps[playback_data->frontal_points.count - 1];
    } else {
        GetLocalTime(&data->playback_last_run_time);
    }
    
    // Update UI
    update_recording_label(data);
    data_class_set_button_state(data, data->play_button_path, false);
    data_class_set_button_state(data, data->pause_button_path, true);
    data_class_set_button_state(data, data->start_record_button_path, false);
    
    return ERROR_NONE;
}

ErrorCode data_class_stop_playback(DataClass* data) {
    if (!data) return ERROR_INVALID_PARAMETER;
    
    data->state = PLAYBACK_COMPLETE;
    data->playback_on = false;
    
    update_recording_label(data);
    data_class_set_button_state(data, data->play_button_path, true);
    data_class_set_button_state(data, data->pause_button_path, false);
    data_class_set_button_state(data, data->start_record_button_path, true);
    
    return ERROR_NONE;
}

ErrorCode data_class_pause_playback(DataClass* data) {
    if (!data) return ERROR_INVALID_PARAMETER;
    
    if (!data->playback_on) return ERROR_INVALID_STATE;
    
    data->state = PLAYBACK_PAUSED;
    update_recording_label(data);
    data_class_set_button_state(data, data->play_button_path, true);
    data_class_set_button_state(data, data->pause_button_path, false);
    
    return ERROR_NONE;
}

ErrorCode data_class_resume_playback(DataClass* data) {
    if (!data) return ERROR_INVALID_PARAMETER;
    
    if (!data->playback_on) return ERROR_INVALID_STATE;
    
    data->state = PLAYBACK;
    GetLocalTime(&data->playback_last_run_time);
    update_recording_label(data);
    data_class_set_button_state(data, data->play_button_path, false);
    data_class_set_button_state(data, data->pause_button_path, true);
    
    return ERROR_NONE;
}

ErrorCode data_class_set_playback_speed(DataClass* data, double speed) {
    if (!data || speed <= 0.0) return ERROR_INVALID_PARAMETER;
    
    data->playback_speed = speed;
    return ERROR_NONE;
}

ErrorCode data_class_mark_playback_complete(DataClass* data) {
    if (!data) return ERROR_INVALID_PARAMETER;
    
    data->state = PLAYBACK_COMPLETE;
    update_recording_label(data);
    return ERROR_NONE;
}

ErrorCode data_class_set_button_state(DataClass* data, 
                                    const char* button_path, 
                                    bool active) {
    if (!data || !button_path) return ERROR_INVALID_PARAMETER;
    
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "%s configure -state {%s}",
             button_path, active ? "normal" : "disabled");
    
    if (Tcl_Eval(data->interp, cmd) != TCL_OK) {
        return ERROR_TCL_EVAL;
    }
    
    return ERROR_NONE;
}

// Getter implementations
bool data_class_is_recording(const DataClass* data) {
    return data ? data->recording_on : false;
}

bool data_class_is_playback(const DataClass* data) {
    return data ? data->playback_on : false;
}

bool data_class_is_paused(const DataClass* data) {
    return data ? data->recording_paused : false;
}

SYSTEMTIME data_class_get_recording_start_time(const DataClass* data) {
    SYSTEMTIME empty_time = {0};
    return data ? data->recording_start_time : empty_time;
}

const SavedData* data_class_get_saved_data(const DataClass* data) {
    return data ? data->saved_data : NULL;
}

DataPoints* data_class_get_frontal_points(DataClass* data) {
    return data ? &data->frontal_points : NULL;
}

DataPoints* data_class_get_sagittal_points(DataClass* data) {
    return data ? &data->sagittal_points : NULL;
}

double data_class_get_playback_speed(const DataClass* data) {
    return data ? data->playback_speed : 1.0;
}

const char* data_class_get_scan_type(const DataClass* data) {
    return data ? data->recording_scan_type : NULL;
}

// Simple helper for time differences in seconds
static double get_time_difference_seconds(const SYSTEMTIME* start, const SYSTEMTIME* end) {
    FILETIME ft1, ft2;
    SystemTimeToFileTime(start, &ft1);
    SystemTimeToFileTime(end, &ft2);
    
    ULARGE_INTEGER uli1, uli2;
    uli1.LowPart = ft1.dwLowDateTime;
    uli1.HighPart = ft1.dwHighDateTime;
    uli2.LowPart = ft2.dwLowDateTime;
    uli2.HighPart = ft2.dwHighDateTime;
    
    return (uli2.QuadPart - uli1.QuadPart) / 10000000.0;
}
