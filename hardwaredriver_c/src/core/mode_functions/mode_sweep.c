#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "mode_sweep.h"
#include "logger.h"
#include "byte_sync.h"
#include "cJSON.h"

// Namespace option functions implementation
ErrorCode set_current_patient_name(const char* patient_name) {
    NamespaceOptions* options;
    ErrorCode error = namespace_options_create(&options, false);
    if (error != ERROR_NONE) return error;
    
    error = namespace_options_set_patient_name(options, patient_name);
    namespace_options_destroy(options);
    return error;
}

ErrorCode toggle_recording(void) {
    NamespaceOptions* options;
    ErrorCode error = namespace_options_create(&options, false);
    if (error != ERROR_NONE) return error;
    
    error = namespace_options_set_event(options, EVENT_TOGGLE_RECORDING, NULL);
    namespace_options_destroy(options);
    return error;
}

ErrorCode start_cms_playback(const char* file_name) {
    NamespaceOptions* options;
    ErrorCode error = namespace_options_create(&options, false);
    if (error != ERROR_NONE) return error;
    
    error = namespace_options_set_event(options, EVENT_CMS_RECORDING_PLAYBACK, file_name);
    namespace_options_destroy(options);
    return error;
}

ErrorCode start_playback(void) {
    NamespaceOptions* options;
    ErrorCode error = namespace_options_create(&options, false);
    if (error != ERROR_NONE) return error;
    
    error = namespace_options_set_event(options, EVENT_CMS_START_PLAYBACK, NULL);
    namespace_options_destroy(options);
    return error;
}

ErrorCode mark_redraw_event(void) {
    NamespaceOptions* options;
    ErrorCode error = namespace_options_create(&options, false);
    if (error != ERROR_NONE) return error;
    
    error = namespace_options_set_event(options, EVENT_MARK_REDRAW_TOOL, NULL);
    namespace_options_destroy(options);
    return error;
}

// PatientNameWatcher implementation
static DWORD WINAPI watch_thread_proc(LPVOID param) {
    PatientNameWatcher* watcher = (PatientNameWatcher*)param;
    char buffer[4096];
    DWORD bytes_returned;
    
    while (watcher->running) {
        if (ReadDirectoryChangesW(
            watcher->dir_handle,
            buffer,
            sizeof(buffer),
            TRUE,
            FILE_NOTIFY_CHANGE_LAST_WRITE,
            &bytes_returned,
            NULL,
            NULL
        )) {
            FILE_NOTIFY_INFORMATION* event = (FILE_NOTIFY_INFORMATION*)buffer;
            if (wcsstr(event->FileName, L"patient_name_options")) {
                watcher->callback(K7_DATA_DIR "\\patient_name_options");
            }
        }
    }
    return 0;
}

ErrorCode patient_name_watcher_create(PatientNameWatcher** watcher, void (*callback)(const char*)) {
    *watcher = (PatientNameWatcher*)malloc(sizeof(PatientNameWatcher));
    if (!*watcher) return ERROR_MEMORY_ALLOCATION;
    
    (*watcher)->dir_handle = CreateFileW(
        L"C:\\K7",
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        NULL
    );
    
    if ((*watcher)->dir_handle == INVALID_HANDLE_VALUE) {
        free(*watcher);
        return ERROR_FILE_OPEN;
    }
    
    (*watcher)->callback = callback;
    (*watcher)->running = true;
    
    (*watcher)->thread_handle = CreateThread(NULL, 0, watch_thread_proc, *watcher, 0, NULL);
    if (!(*watcher)->thread_handle) {
        CloseHandle((*watcher)->dir_handle);
        free(*watcher);
        return ERROR_THREAD_CREATE;
    }
    
    return ERROR_NONE;
}

void patient_name_watcher_destroy(PatientNameWatcher* watcher) {
    if (!watcher) return;
    
    watcher->running = false;
    if (watcher->thread_handle) {
        WaitForSingleObject(watcher->thread_handle, INFINITE);
        CloseHandle(watcher->thread_handle);
    }
    if (watcher->dir_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(watcher->dir_handle);
    }
    free(watcher);
}

// ModeSweep VTable function implementations
static int get_mode_number(const ModeBase* mode) {
    (void)mode;
    return 50;  // Sweep mode is 50
}

static const uint8_t* get_emg_config(const ModeBase* mode, size_t* length) {
    (void)mode;  // Sweep mode always uses 'r' config
    static const uint8_t config[] = {'r'};
    *length = 1;
    return config;
}

static ErrorCode execute_mode(ModeBase* base, uint8_t* output, size_t* output_length) {
    if (!base || !output || !output_length) return ERROR_INVALID_PARAMETER;
    
    ModeSweep* mode = (ModeSweep*)base->impl;
    
    // Handle first run initialization
    if (mode->is_first_run) {
        uint8_t read_buffer[MODE_SWEEP_READ_SIZE];
        size_t bytes_read;
        
        // Read and discard initial data
        for (int i = 0; i < 5; i++) {
            ErrorCode error = serial_interface_read_data(base->interface, 
                                                       read_buffer, 
                                                       MODE_SWEEP_READ_SIZE, 
                                                       &bytes_read);
            if (error != ERROR_NONE) continue;
        }
        mode->is_first_run = false;
    }

    // Read data
    ErrorCode error = serial_interface_read_data(base->interface, 
                                               output, 
                                               MODE_SWEEP_READ_SIZE, 
                                               output_length);
    if (error != ERROR_NONE) return error;

    // Process the data
    return mode_sweep_process_data(mode, output, *output_length);
}

static ErrorCode execute_mode_not_connected(ModeBase* base, uint8_t* output, size_t* output_length) {
    if (!output || !output_length) return ERROR_INVALID_PARAMETER;

    // Match Python's pattern: [0, 0, 1 << 4, 0, 2 << 4, 0, 3 << 4, 0] + [0] * 8
    static const uint8_t pattern[] = {
        0, 0, 16, 0, 32, 0, 48, 0,  // CMS pattern
        0, 0, 0, 0, 0, 0, 0, 0      // Tilt zeros
    };

    size_t pattern_size = sizeof(pattern);
    size_t max_repeats = MODE_SWEEP_READ_SIZE / pattern_size;
    
    for (size_t i = 0; i < max_repeats; i++) {
        memcpy(output + (i * pattern_size), pattern, pattern_size);
    }
    
    *output_length = max_repeats * pattern_size;
    return ERROR_NONE;
}

static const ModeBaseVTable mode_sweep_vtable = {
    .get_mode_number = get_mode_number,
    .get_emg_config = get_emg_config,
    .execute_mode = execute_mode,
    .execute_mode_not_connected = execute_mode_not_connected,
    .stop = NULL,
    .destroy = NULL
};

// Core functionality implementation
ErrorCode mode_sweep_compute_angle(double ref, double axis1, double axis2, double* result) {
    if (!result) return ERROR_INVALID_PARAMETER;
    
    double sum = sqrt((axis1 * axis1) + (axis2 * axis2));
    if (sum == 0) {
        *result = 0;
        return ERROR_NONE;
    }

    *result = atan(ref / sum) * SCALING_FACTOR;
    if (ref < 0) {
        *result = -1 * (*result);
    }
    return ERROR_NONE;
}

ErrorCode mode_sweep_compute_tilt_data(const uint8_t* tilt_values, double* front_angle, double* side_angle) {
    if (!tilt_values || !front_angle || !side_angle) return ERROR_INVALID_PARAMETER;

    int16_t computed[4] = {0};
    
    for (int i = 0; i < 8; i += 2) {
        uint8_t n_low = tilt_values[i];
        uint8_t n_high = tilt_values[i + 1];

        int16_t scaled_value = ((n_low & 255) << 8) | n_high;
        if (i < 6 && (scaled_value & 0x8000)) {  // Check negative number for first 3 values
            scaled_value = ((scaled_value ^ 0xFFFF) + 1) * -1;
        }
        computed[i/2] = scaled_value;
    }

    ErrorCode error = mode_sweep_compute_angle(computed[1], computed[0], computed[2], front_angle);
    if (error != ERROR_NONE) return error;

    if (computed[1] < 0) {
        *front_angle = -1 * (*front_angle);
    }

    error = mode_sweep_compute_angle(computed[2], computed[0], computed[1], side_angle);
    if (error != ERROR_NONE) return error;

    if (computed[2] < 0) {
        *side_angle = *side_angle;
    } else {
        *side_angle = -1 * (*side_angle);
    }

    return ERROR_NONE;
}

ErrorCode mode_sweep_process_data(ModeSweep* mode, const uint8_t* data, size_t length) {
    if (!mode || !data || length == 0) return ERROR_INVALID_PARAMETER;

    SyncResult sync_result;
    sync_result_init(&sync_result);

    ErrorCode error = resync_bytes(data, length, MODE_SWEEP_BLOCK_SIZE,
                                 sync_cms_channels, sync_tilt_channels,
                                 0, 8, &sync_result);
    if (error != ERROR_NONE) {
        sync_result_free(&sync_result);
        return error;
    }

    if (!sync_result.found_sync || sync_result.synced_length == 0) {
        sync_result_free(&sync_result);
        return ERROR_INVALID_DATA;
    }

    // Process tilt data for each block
    for (size_t i = 0; i < sync_result.synced_length; i += MODE_SWEEP_BLOCK_SIZE) {
        double front_angle, side_angle;
        error = mode_sweep_compute_tilt_data(sync_result.synced_data + i + 8,
                                           &front_angle, &side_angle);
        if (error != ERROR_NONE) continue;

        // Queue the data for UI processing
        double angles[2] = {front_angle, side_angle};
        error = data_queue_put(mode->sweep_queue, angles, 2);
        if (error != ERROR_NONE) {
            log_warning("Failed to queue sweep data");
        }
    }

    sync_result_free(&sync_result);
    return ERROR_NONE;
}

// Constructor/Destructor implementation
ErrorCode mode_sweep_create(ModeSweep** mode, SerialInterface* interface, 
                          ProcessManager* process_manager,
                          bool show_tilt_window, bool show_sweep_graph) {
    if (!mode || !interface || !process_manager) {
        return ERROR_INVALID_PARAMETER;
    }

    ModeSweep* new_mode = (ModeSweep*)calloc(1, sizeof(ModeSweep));
    if (!new_mode) {
        return ERROR_MEMORY_ALLOCATION;
    }

    ErrorCode error = mode_base_create(&new_mode->base, interface, process_manager,
                                     &mode_sweep_vtable, new_mode);
    if (error != ERROR_NONE) {
        free(new_mode);
        return error;
    }

    // Initialize data queue
    new_mode->sweep_queue = data_queue_create(1000);
    if (!new_mode->sweep_queue) {
        mode_base_destroy(&new_mode->base);
        free(new_mode);
        return ERROR_MEMORY_ALLOCATION;
    }

    // Initialize namespace options
    error = namespace_options_create(&new_mode->namespace, false);
    if (error != ERROR_NONE) {
        data_queue_destroy(new_mode->sweep_queue);
        mode_base_destroy(&new_mode->base);
        free(new_mode);
        return error;
    }

    // Create patient name watcher
    error = patient_name_watcher_create(&new_mode->watcher, 
                                      (void(*)(const char*))namespace_options_load_from_directory);
    if (error != ERROR_NONE) {
        namespace_options_destroy(new_mode->namespace);
        data_queue_destroy(new_mode->sweep_queue);
        mode_base_destroy(&new_mode->base);
        free(new_mode);
        return error;
    }

    new_mode->is_first_run = true;
    new_mode->show_tilt_window = show_tilt_window;
    new_mode->show_sweep_graph = show_sweep_graph;
    new_mode->plot_app = NULL;  // UI placeholder

    *mode = new_mode;
    return ERROR_NONE;
}

void mode_sweep_destroy(ModeSweep* mode) {
    if (!mode) return;

    if (mode->watcher) {
        patient_name_watcher_destroy(mode->watcher);
    }
    if (mode->namespace) {
        namespace_options_destroy(mode->namespace);
    }
    if (mode->sweep_queue) {
        data_queue_destroy(mode->sweep_queue);
    }
    
    mode_base_destroy(&mode->base);
    free(mode);
}

// UI Integration placeholder implementations
ErrorCode mode_sweep_start(ModeSweep* mode) {
    if (!mode) return ERROR_INVALID_PARAMETER;
    // Placeholder for UI initialization
    return ERROR_NONE;
}

ErrorCode mode_sweep_stop(ModeSweep* mode) {
    if (!mode) return ERROR_INVALID_PARAMETER;
    // Placeholder for UI cleanup
    return ERROR_NONE;
}

ErrorCode mode_sweep_save_mode_type(bool show_sweep_graph) {
    cJSON* root = cJSON_CreateObject();
    if (!root) return ERROR_MEMORY_ALLOCATION;

    cJSON_AddBoolToObject(root, "show_sweep_graph", show_sweep_graph);
    
    char* json_str = cJSON_Print(root);
    cJSON_Delete(root);
    
    if (!json_str) return ERROR_MEMORY_ALLOCATION;

    FILE* file = fopen(K7_MODE_TYPE_PATH, "w");
    if (!file) {
        free(json_str);
        return ERROR_FILE_OPEN;
    }

    fprintf(file, "%s", json_str);
    fclose(file);
    free(json_str);
    
    return ERROR_NONE;
}