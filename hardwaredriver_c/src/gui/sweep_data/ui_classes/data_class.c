#include "data_class.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "logger.h"
#include "../utils.h"

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
    time_t recording_start_time;
    char* recording_scan_type;
    
    // Playback state
    bool playback_on;
    SavedData* playback_data;
    time_t playback_last_run_time;
    double playback_speed;
    bool fast_replay;
    
    // UI elements
    ImageWindowOptions* window_options;
    char* status_label_path;
    char* play_button_path;
    char* pause_button_path;
    char* record_button_path;
    char* save_button_path;
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

static void setup_ui_paths(DataClass* data) {
    char path[256];
    snprintf(path, sizeof(path), "%s.current_status", data->builder_path);
    data->status_label_path = strdup(path);
    
    snprintf(path, sizeof(path), "%s.play_button", data->builder_path);
    data->play_button_path = strdup(path);
    
    snprintf(path, sizeof(path), "%s.pause_button", data->builder_path);
    data->pause_button_path = strdup(path);
    
    snprintf(path, sizeof(path), "%s.record_button", data->builder_path);
    data->record_button_path = strdup(path);
    
    snprintf(path, sizeof(path), "%s.save_button", data->builder_path);
    data->save_button_path = strdup(path);
}

static void update_recording_label(DataClass* data) {
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

static void set_button_states(DataClass* data, 
                            bool start_enabled,
                            bool stop_enabled,
                            bool save_enabled) {
    char cmd[256];
    
    snprintf(cmd, sizeof(cmd), "%s configure -state %s",
             data->record_button_path, start_enabled ? "normal" : "disabled");
    Tcl_Eval(data->interp, cmd);
    
    snprintf(cmd, sizeof(cmd), "%s configure -state %s",
             data->pause_button_path, stop_enabled ? "normal" : "disabled");
    Tcl_Eval(data->interp, cmd);
    
    snprintf(cmd, sizeof(cmd), "%s configure -state %s",
             data->save_button_path, save_enabled ? "normal" : "disabled");
    Tcl_Eval(data->interp, cmd);
}

static ErrorCode compute_rolling_average(const DataPoints* input,
                                       size_t points_count,
                                       DataPoints* output) {
    if (!input || !output || input->count == 0) 
        return ERROR_INVALID_PARAMETER;
        
    double window_size = 0.1; // 0.1 second windows
    double start_time = input->timestamps[0];
    double end_time = input->timestamps[input->count-1];
    size_t window_count = (size_t)ceil((end_time - start_time) / window_size);
    
    double* avg_values = calloc(window_count, sizeof(double));
    double* avg_times = calloc(window_count, sizeof(double));
    double* window_sums = calloc(window_count, sizeof(double));
    size_t* window_counts = calloc(window_count, sizeof(size_t));
    
    if (!avg_values || !avg_times || !window_sums || !window_counts) {
        free(avg_values);
        free(avg_times);
        free(window_sums);
        free(window_counts);
        return ERROR_OUT_OF_MEMORY;
    }
    
    for (size_t i = 0; i < window_count; i++) {
        avg_times[i] = start_time + (i * window_size) + (window_size / 2);
    }
    
    for (size_t i = 0; i < input->count; i++) {
        double time = input->timestamps[i];
        size_t window_idx = (size_t)((time - start_time) / window_size);
        if (window_idx < window_count) {
            window_sums[window_idx] += input->values[i];
            window_counts[window_idx]++;
        }
    }
    
    size_t valid_windows = 0;
    for (size_t i = 0; i < window_count; i++) {
        if (window_counts[i] > 0) {
            avg_values[valid_windows] = window_sums[i] / window_counts[i];
            avg_times[valid_windows] = avg_times[i];
            valid_windows++;
        }
    }
    
    ErrorCode err = ensure_capacity(output, valid_windows);
    if (err != ERROR_NONE) {
        free(avg_values);
        free(avg_times);
        free(window_sums);
        free(window_counts);
        return err;
    }
    
    memcpy(output->values, avg_values, valid_windows * sizeof(double));
    memcpy(output->timestamps, avg_times, valid_windows * sizeof(double));
    output->count = valid_windows;
    
    free(avg_values);
    free(avg_times);
    free(window_sums);
    free(window_counts);
    
    return ERROR_NONE;
}

DataClass* data_class_create(Tcl_Interp* interp, 
                           const char* builder_path,
                           const char* namespace_path,
                           DataQueue* data_queue) {
    if (!interp || !builder_path || !namespace_path || !data_queue) return NULL;
    
    DataClass* data = calloc(1, sizeof(DataClass));
    if (!data) return NULL;
    
    data->interp = interp;
    data->builder_path = strdup(builder_path);
    data->namespace_path = strdup(namespace_path);
    data->data_queue = data_queue;
    
    data->window_options = image_window_options_create();
    if (!data->window_options) {
        data_class_destroy(data);
        return NULL;
    }
    
    setup_ui_paths(data);
    init_data_points(&data->frontal_points);
    init_data_points(&data->sagittal_points);
    
    data->state = NOT_RECORDING;
    data->playback_speed = 1.0;
    
    return data;
}

void data_class_destroy(DataClass* data) {
    if (!data) return;
    
    free(data->builder_path);
    free(data->namespace_path);
    free(data->recording_scan_type);
    
    free_data_points(&data->frontal_points);
    free_data_points(&data->sagittal_points);
    
    if (data->saved_data) {
        free_data_points(&data->saved_data->frontal_points);
        free_data_points(&data->saved_data->sagittal_points);
        free(data->saved_data->scan_type);
        free(data->saved_data);
    }
    
    if (data->saved_data_points) {
        for (size_t i = 0; i < data->saved_data_count; i++) {
            if (data->saved_data_points[i]) {
                free_data_points(&data->saved_data_points[i]->frontal_points);
                free_data_points(&data->saved_data_points[i]->sagittal_points);
                free(data->saved_data_points[i]->scan_type);
                free(data->saved_data_points[i]);
            }
        }
        free(data->saved_data_points);
    }
    
    if (data->playback_data) {
        free_data_points(&data->playback_data->frontal_points);
        free_data_points(&data->playback_data->sagittal_points);
        free(data->playback_data->scan_type);
        free(data->playback_data);
    }
    
    image_window_options_destroy(data->window_options);
    
    free(data->status_label_path);
    free(data->play_button_path);
    free(data->pause_button_path);
    free(data->record_button_path);
    free(data->save_button_path);
    
    free(data);
}

ErrorCode data_class_start_recording(DataClass* data, const char* scan_type_label) {
    if (!data || !scan_type_label) return ERROR_INVALID_PARAMETER;
    
    ErrorCode err = data_class_clear_all(data, !data->recording_paused);
    if (err != ERROR_NONE) return err;
    
    free(data->recording_scan_type);
    data->recording_scan_type = strdup(scan_type_label);
    
    data->state = RECORDING_ON;
    data->recording_on = true;
    data->recording_paused = false;
    time(&data->recording_start_time);
    
    update_recording_label(data);
    set_button_states(data, false, true, false);
    
    return ERROR_NONE;
}

ErrorCode data_class_stop_recording(DataClass* data) {
    if (!data) return ERROR_INVALID_PARAMETER;
    
    data->state = RECORDING_COMPLETE;
    data->recording_on = true;
    data->recording_paused = true;
    
    update_recording_label(data);
    set_button_states(data, true, false, true);
    
    // Save current data points as saved data
    if (data->saved_data) {
        free(data->saved_data);
    }
    
    data->saved_data = calloc(1, sizeof(SavedData));
    if (!data->saved_data) return ERROR_OUT_OF_MEMORY;
    
    data->saved_data->frontal_points = data->frontal_points;
    data->saved_data->sagittal_points = data->sagittal_points;
    data->saved_data->scan_type = strdup(data->recording_scan_type);
    
    init_data_points(&data->frontal_points);
    init_data_points(&data->sagittal_points);
    
    return ERROR_NONE;
}

ErrorCode data_class_toggle_recording(DataClass* data, const char* scan_type_label) {
    if (!data || !scan_type_label) return ERROR_INVALID_PARAMETER;
    
    if (data->recording_on && !data->recording_paused) {
        return data_class_stop_recording(data);
    } else {
        return data_class_start_recording(data, scan_type_label);
    }
}

ErrorCode data_class_pause_data_capture(DataClass* data) {
    if (!data) return ERROR_INVALID_PARAMETER;
    data->recording_paused = true;
    return ERROR_NONE;
}

ErrorCode data_class_resume_data_capture(DataClass* data) {
    if (!data) return ERROR_INVALID_PARAMETER;
    data->recording_paused = false;
    return ERROR_NONE;
}

ErrorCode data_class_clear_all(DataClass* data, bool clear_all) {
    if (!data) return ERROR_INVALID_PARAMETER;
    
    data->recording_paused = false;
    data->playback_on = false;
    
    if (clear_all) {
        data_class_reinitialize_y_points(data);
    }
    
    image_window_options_reset_min_max_values(data->window_options);
    
    data->state = NOT_RECORDING;
    update_recording_label(data);
    
    if (data->playback_data) {
        free_data_points(&data->playback_data->frontal_points);
        free_data_points(&data->playback_data->sagittal_points);
        free(data->playback_data->scan_type);
        free(data->playback_data);
        data->playback_data = NULL;
    }
    
    data->playback_last_run_time = 0;
    
    if (data->saved_data) {
        free_data_points(&data->saved_data->frontal_points);
        free_data_points(&data->saved_data->sagittal_points);
        free(data->saved_data->scan_type);
        free(data->saved_data);
        data->saved_data = NULL;
    }
    
    // Clear queue
    while (data_queue_size(data->data_queue) > 0) {
        double data_point;
        size_t count = 1;
        data_queue_get(data->data_queue, &data_point, &count);
    }
    
    data->recording_on = false;
    data->recording_start_time = 0;
    free(data->recording_scan_type);
    data->recording_scan_type = NULL;
    
    set_button_states(data, true, false, false);
    
    return ERROR_NONE;
}

ErrorCode data_class_save_recording(DataClass* data, 
                                  const char* patient_path, 
                                  const char* extra_filter) {
    if (!data || !patient_path) return ERROR_INVALID_PARAMETER;
    
    if (data->recording_on) {
        if (!data->recording_paused) {
            ErrorCode err = data_class_stop_recording(data);
            if (err != ERROR_NONE) return err;
        }
    } else {
        data->state = ERROR_NOT_RECORDING;
        update_recording_label(data);
        return ERROR_INVALID_STATE;
    }
    
    char filename[256];
    encode_curr_datetime(filename, sizeof(filename));
    
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s/%s.dat", patient_path, filename);
    
    FILE* file = fopen(full_path, "wb");
    if (!file) return ERROR_FILE_OPERATION;
    
    // Write saved data
    if (data->saved_data) {
        fwrite(&data->saved_data->frontal_points.count, sizeof(size_t), 1, file);
        fwrite(data->saved_data->frontal_points.values, 
               sizeof(double), 
               data->saved_data->frontal_points.count, 
               file);
        fwrite(data->saved_data->frontal_points.timestamps, 
               sizeof(double), 
               data->saved_data->frontal_points.count, 
               file);
               
        fwrite(&data->saved_data->sagittal_points.count, sizeof(size_t), 1, file);
        fwrite(data->saved_data->sagittal_points.values, 
               sizeof(double), 
               data->saved_data->sagittal_points.count, 
               file);
        fwrite(data->saved_data->sagittal_points.timestamps, 
               sizeof(double), 
               data->saved_data->sagittal_points.count, 
               file);
               
        size_t scan_type_len = strlen(data->saved_data->scan_type);
        fwrite(&scan_type_len, sizeof(size_t), 1, file);
        fwrite(data->saved_data->scan_type, sizeof(char), scan_type_len, file);
    }
    
    // Write extra filter if provided
    if (extra_filter) {
        size_t filter_len = strlen(extra_filter);
        fwrite(&filter_len, sizeof(size_t), 1, file);
        fwrite(extra_filter, sizeof(char), filter_len, file);
    } else {
        size_t filter_len = 0;
        fwrite(&filter_len, sizeof(size_t), 1, file);
    }
    
    fclose(file);
    
    data->recording_on = false;
    data->recording_paused = false;
    
    return ERROR_NONE;
}

ErrorCode data_class_append_data(DataClass* data, 
                               const double* y_points, 
                               size_t points_count) {
    if (!data || !y_points || points_count == 0) return ERROR_INVALID_PARAMETER;
    
    // Ensure capacity for new points
    ErrorCode err = ensure_capacity(&data->frontal_points, 
                                  data->frontal_points.count + points_count);
    if (err != ERROR_NONE) return err;
    
    err = ensure_capacity(&data->sagittal_points, 
                         data->sagittal_points.count + points_count);
    if (err != ERROR_NONE) return err;
    
    // Get current time for timestamps
    time_t now;
    time(&now);
    double timestamp = difftime(now, data->recording_start_time);
    
    // Add new points
    for (size_t i = 0; i < points_count; i++) {
        size_t idx = data->frontal_points.count;
        data->frontal_points.values[idx] = y_points[i * 2];
        data->frontal_points.timestamps[idx] = timestamp;
        data->sagittal_points.values[idx] = y_points[i * 2 + 1];
        data->sagittal_points.timestamps[idx] = timestamp;
        data->frontal_points.count++;
        data->sagittal_points.count++;
    }
    
    return ERROR_NONE;
}

ErrorCode data_class_set_image_window_values(DataClass* data, 
                                           double frontal, 
                                           double sagittal) {
    if (!data) return ERROR_INVALID_PARAMETER;
    
    image_window_options_set_current_values(data->window_options, frontal, sagittal);
    return ERROR_NONE;
}

ErrorCode data_class_reinitialize_y_points(DataClass* data) {
    if (!data) return ERROR_INVALID_PARAMETER;
    
    free_data_points(&data->frontal_points);
    free_data_points(&data->sagittal_points);
    init_data_points(&data->frontal_points);
    init_data_points(&data->sagittal_points);
    
    return ERROR_NONE;
}

ErrorCode data_class_start_playback(DataClass* data, const char* filename) {
    if (!data || !filename) return ERROR_INVALID_PARAMETER;
    
    FILE* file = fopen(filename, "rb");
    if (!file) return ERROR_FILE_OPERATION;
    
    SavedData* playback = calloc(1, sizeof(SavedData));
    if (!playback) {
        fclose(file);
        return ERROR_OUT_OF_MEMORY;
    }
    
    // Read frontal points
    size_t count;
    if (fread(&count, sizeof(size_t), 1, file) != 1) {
        free(playback);
        fclose(file);
        return ERROR_FILE_OPERATION;
    }
    
    playback->frontal_points.values = malloc(count * sizeof(double));
    playback->frontal_points.timestamps = malloc(count * sizeof(double));
    if (!playback->frontal_points.values || !playback->frontal_points.timestamps) {
        free(playback->frontal_points.values);
        free(playback->frontal_points.timestamps);
        free(playback);
        fclose(file);
        return ERROR_OUT_OF_MEMORY;
    }
    
    if (fread(playback->frontal_points.values, sizeof(double), count, file) != count ||
        fread(playback->frontal_points.timestamps, sizeof(double), count, file) != count) {
        free(playback->frontal_points.values);
        free(playback->frontal_points.timestamps);
        free(playback);
        fclose(file);
        return ERROR_FILE_OPERATION;
    }
    playback->frontal_points.count = count;
    playback->frontal_points.capacity = count;
    
    // Read sagittal points
    if (fread(&count, sizeof(size_t), 1, file) != 1) {
        free_data_points(&playback->frontal_points);
        free(playback);
        fclose(file);
        return ERROR_FILE_OPERATION;
    }
    
    playback->sagittal_points.values = malloc(count * sizeof(double));
    playback->sagittal_points.timestamps = malloc(count * sizeof(double));
    if (!playback->sagittal_points.values || !playback->sagittal_points.timestamps) {
        free_data_points(&playback->frontal_points);
        free(playback->sagittal_points.values);
        free(playback->sagittal_points.timestamps);
        free(playback);
        fclose(file);
        return ERROR_OUT_OF_MEMORY;
    }
    
    if (fread(playback->sagittal_points.values, sizeof(double), count, file) != count ||
        fread(playback->sagittal_points.timestamps, sizeof(double), count, file) != count) {
        free_data_points(&playback->frontal_points);
        free_data_points(&playback->sagittal_points);
        free(playback);
        fclose(file);
        return ERROR_FILE_OPERATION;
    }
    playback->sagittal_points.count = count;
    playback->sagittal_points.capacity = count;
    
    // Read scan type
    size_t scan_type_len;
    if (fread(&scan_type_len, sizeof(size_t), 1, file) != 1) {
        free_data_points(&playback->frontal_points);
        free_data_points(&playback->sagittal_points);
        free(playback);
        fclose(file);
        return ERROR_FILE_OPERATION;
    }
    
    playback->scan_type = malloc(scan_type_len + 1);
    if (!playback->scan_type) {
        free_data_points(&playback->frontal_points);
        free_data_points(&playback->sagittal_points);
        free(playback);
        fclose(file);
        return ERROR_OUT_OF_MEMORY;
    }
    
    if (fread(playback->scan_type, sizeof(char), scan_type_len, file) != scan_type_len) {
        free_data_points(&playback->frontal_points);
        free_data_points(&playback->sagittal_points);
        free(playback->scan_type);
        free(playback);
        fclose(file);
        return ERROR_FILE_OPERATION;
    }
    playback->scan_type[scan_type_len] = '\0';
    
    fclose(file);
    
    // Set up playback state
    if (data->playback_data) {
        free_data_points(&data->playback_data->frontal_points);
        free_data_points(&data->playback_data->sagittal_points);
        free(data->playback_data->scan_type);
        free(data->playback_data);
    }
    
    data->playback_data = playback;
    data->state = PLAYBACK;
    data->playback_on = true;
    time(&data->playback_last_run_time);
    
    update_recording_label(data);
    set_button_states(data, false, true, false);
    
    return ERROR_NONE;
}

ErrorCode data_class_stop_playback(DataClass* data) {
    if (!data) return ERROR_INVALID_PARAMETER;
    
    data->state = PLAYBACK_COMPLETE;
    data->playback_on = false;
    
    update_recording_label(data);
    set_button_states(data, true, false, false);
    
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
                                    const char* button_name, 
                                    bool active) {
    if (!data || !button_name) return ERROR_INVALID_PARAMETER;
    
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "%s configure -state %s",
             button_name, active ? "normal" : "disabled");
    Tcl_Eval(data->interp, cmd);
    
    return ERROR_NONE;
}

bool data_class_is_recording(const DataClass* data) {
    return data ? data->recording_on : false;
}

bool data_class_is_playback(const DataClass* data) {
    return data ? data->playback_on : false;
}

bool data_class_is_paused(const DataClass* data) {
    return data ? data->recording_paused : false;
}

time_t data_class_get_recording_start_time(const DataClass* data) {
    return data ? data->recording_start_time : 0;
}

const SavedData* data_class_get_saved_data(const DataClass* data) {
    return data ? data->saved_data : NULL;
}

const DataPoints* data_class_get_frontal_points(const DataClass* data) {
    return data ? &data->frontal_points : NULL;
}

const DataPoints* data_class_get_sagittal_points(const DataClass* data) {
    return data ? &data->sagittal_points : NULL;
}

double data_class_get_playback_speed(const DataClass* data) {
    return data ? data->playback_speed : 1.0;
}

const char* data_class_get_scan_type(const DataClass* data) {
    return data ? data->recording_scan_type : NULL;
}



