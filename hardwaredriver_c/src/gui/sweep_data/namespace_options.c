#include "namespace_options.h"
#include "serialize_deserialize.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <direct.h>
#include "display_tilt_supplemental_windows.h"

#define OPTIONS_FILENAME "namespace_options"
#define PATIENT_NAME_FILENAME "patient_name_options"
#define PATIENT_DRIVE_FILENAME "patient_drive_options"

static char get_full_path[MAX_PATH_LENGTH];

static ErrorCode get_full_filepath(const char* filename, char* filepath, size_t max_length) {
    snprintf(filepath, max_length, "%s\\%s", K7_DATA_DIR, filename);
    return ERROR_NONE;
}

static ErrorCode create_directory_if_not_exists(const char* path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        if (_mkdir(path) == -1) {
            log_error("Failed to create directory: %s", path);
            return ERROR_FILE_CREATE;
        }
    }
    return ERROR_NONE;
}

static ErrorCode set_patient_path(NamespaceOptions* options, const char* patient_name) {
    char* first_name = strdup(patient_name);
    char* last_name = strchr(first_name, '+');
    if (!last_name) {
        free(first_name);
        return ERROR_INVALID_FORMAT;
    }
    
    *last_name = '\0';
    last_name++;
    
    strncpy(options->first_name, first_name, MAX_NAME_LENGTH - 1);
    strncpy(options->last_name, last_name, MAX_NAME_LENGTH - 1);
    
    char root_dir[MAX_PATH_LENGTH];
    ErrorCode error = namespace_options_get_root_data_dir(root_dir, MAX_PATH_LENGTH);
    if (error != ERROR_NONE) {
        free(first_name);
        return error;
    }
    
    snprintf(options->patient_path, MAX_PATH_LENGTH, "%s\\%s\\%s", 
             root_dir, last_name, first_name);
    
    error = create_directory_if_not_exists(options->patient_path);
    free(first_name);
    return error;
}

static DWORD WINAPI watch_directory(LPVOID param) {
    NamespaceOptions* options = (NamespaceOptions*)param;
    char buffer[BUFFER_SIZE];
    DWORD bytes_returned;
    
    while (options->should_run) {
        if (ReadDirectoryChangesW(
                options->dir_handle,
                buffer,
                BUFFER_SIZE,
                TRUE,
                FILE_NOTIFY_CHANGE_LAST_WRITE,
                &bytes_returned,
                &options->overlapped,
                NULL)) {
            
            WaitForSingleObject(options->overlapped.hEvent, INFINITE);
            
            FILE_NOTIFY_INFORMATION* event = (FILE_NOTIFY_INFORMATION*)buffer;
            do {
                // Convert filename to char array
                char filename[MAX_PATH];
                WideCharToMultiByte(CP_UTF8, 0, event->FileName,
                                  event->FileNameLength / sizeof(WCHAR),
                                  filename, MAX_PATH, NULL, NULL);
                filename[event->FileNameLength / sizeof(WCHAR)] = '\0';
                
                if (strstr(filename, "options_display")) {
                    bool show_windows;
                    if (read_config_tilt_supplemental_windows(&show_windows) == ERROR_NONE) {
                        options->options_display = show_windows;
                        if (options->options_display_callback) {
                            options->options_display_callback(show_windows);
                        }
                    }
                } else if (strstr(filename, "namespace_options")) {
                    if (options->namespace_callback) {
                        options->namespace_callback();
                    }
                } else if (strstr(filename, "patient_name_options")) {
                    if (options->patient_name_callback) {
                        options->patient_name_callback(filename);
                    }
                } else if (strstr(filename, "sweep_data")) {
                    if (options->user_data_callback) {
                        options->user_data_callback(filename);
                    }
                }
                
                event = (FILE_NOTIFY_INFORMATION*)((LPBYTE)event + event->NextEntryOffset);
            } while (event->NextEntryOffset);
        }
    }
    return 0;
}

ErrorCode namespace_options_create(NamespaceOptions** options, bool reset_states) {
    *options = (NamespaceOptions*)calloc(1, sizeof(NamespaceOptions));
    if (!*options) return ERROR_MEMORY_ALLOCATION;
    
    // Initialize options_display from config
    ErrorCode error = read_config_tilt_supplemental_windows(&(*options)->options_display);
    if (error != ERROR_NONE) {
        free(*options);
        return error;
    }
    
    if (reset_states) {
        (*options)->exit_thread = false;
        (*options)->app_ready = false;
        memset((*options)->event, 0, MAX_EVENT_LENGTH);
        memset((*options)->event_data, 0, MAX_EVENT_DATA_LENGTH);
        
        // Save the reset state
        ErrorCode error = namespace_options_save_to_directory(*options);
        if (error != ERROR_NONE) {
            free(*options);
            *options = NULL;
            return error;
        }
    }
    
    return namespace_options_load_from_directory(*options);
}

void namespace_options_destroy(NamespaceOptions* options) {
    if (!options) return;

    // Stop watching thread
    options->should_run = FALSE;
    
    if (options->thread_handle) {
        CancelIoEx(options->dir_handle, &options->overlapped);
        WaitForSingleObject(options->thread_handle, INFINITE);
        CloseHandle(options->thread_handle);
        CloseHandle(options->overlapped.hEvent);
        CloseHandle(options->dir_handle);
    }

    free(options);
}

ErrorCode namespace_options_get_patient_name(NamespaceOptions* options, char* name, size_t max_length) {
    char filepath[MAX_PATH_LENGTH];
    ErrorCode error = get_full_filepath(PATIENT_NAME_FILENAME, filepath, MAX_PATH_LENGTH);
    if (error != ERROR_NONE) return error;

    FILE* file = fopen(filepath, "rb");
    if (!file) return ERROR_FILE_OPEN;

    size_t read_size = fread(name, 1, max_length - 1, file);
    name[read_size] = '\0';
    
    fclose(file);
    return ERROR_NONE;
}

ErrorCode namespace_options_set_patient_name(NamespaceOptions* options, const char* name) {
    ErrorCode error = write_patient_name(name);  // Existing functionality
    if (error != ERROR_NONE) return error;

    error = set_patient_path(options, name);  // Existing functionality
    if (error != ERROR_NONE) return error;

    // Reset file watching when patient changes
    if (options->dir_handle) {
        namespace_options_destroy(options);
        error = namespace_options_setup_watch(options, 
                                           options->options_display_callback,
                                           options->namespace_callback,
                                           options->patient_name_callback);
        if (error != ERROR_NONE) return error;

        error = namespace_options_setup_user_data_watch(options,
                                                     options->user_data_callback);
    }

    return error;
}

ErrorCode namespace_options_get_event(NamespaceOptions* options, char* event, size_t max_event_length, 
                                    char* event_data, size_t max_data_length) {
    if (!options->event[0]) {
        event[0] = '\0';
        event_data[0] = '\0';
        return ERROR_NONE;
    }

    strncpy(event, options->event, max_event_length - 1);
    strncpy(event_data, options->event_data, max_data_length - 1);
    
    // Clear the event after reading
    memset(options->event, 0, MAX_EVENT_LENGTH);
    memset(options->event_data, 0, MAX_EVENT_DATA_LENGTH);
    
    return namespace_options_save_to_directory(options);
}

ErrorCode namespace_options_set_event(NamespaceOptions* options, const char* event, const char* event_data) {
    // Don't override existing events except for specific cases
    if (options->event[0] && 
        strcmp(event, EVENT_USER_RECORD_SAVED) != 0 && 
        strcmp(event, EVENT_TOGGLE_RECORDING) != 0 && 
        strcmp(event, EVENT_MARK_REDRAW_TOOL) != 0) {
        return ERROR_NONE;
    }

    strncpy(options->event, event, MAX_EVENT_LENGTH - 1);
    if (event_data) {
        strncpy(options->event_data, event_data, MAX_EVENT_DATA_LENGTH - 1);
    } else {
        options->event_data[0] = '\0';
    }

    return namespace_options_save_to_directory(options);
}

ErrorCode namespace_options_save_to_directory(const NamespaceOptions* options) {
    char filepath[MAX_PATH_LENGTH];
    ErrorCode error = get_full_filepath(OPTIONS_FILENAME, filepath, MAX_PATH_LENGTH);
    if (error != ERROR_NONE) return error;

    FILE* file = fopen(filepath, "wb");
    if (!file) return ERROR_FILE_OPEN;

    AppState state = {
        .exit_thread = options->exit_thread,
        .app_ready = options->app_ready
    };
    strncpy(state.event, options->event, sizeof(state.event) - 1);
    strncpy(state.event_data, options->event_data, sizeof(state.event_data) - 1);
    strncpy(state.requested_playback_file, options->requested_playback_file, 
            sizeof(state.requested_playback_file) - 1);

    error = app_state_serialize(filepath, &state);
    fclose(file);
    return error;
}

ErrorCode namespace_options_load_from_directory(NamespaceOptions* options) {
    char filepath[MAX_PATH_LENGTH];
    ErrorCode error = get_full_filepath(OPTIONS_FILENAME, filepath, MAX_PATH_LENGTH);
    if (error != ERROR_NONE) return error;

    AppState state;
    error = app_state_deserialize(filepath, &state);
    if (error != ERROR_NONE) {
        if (error == ERROR_FILE_OPEN) {
            // File doesn't exist, create with default values
            return namespace_options_save_to_directory(options);
        }
        return error;
    }

    options->exit_thread = state.exit_thread;
    options->app_ready = state.app_ready;
    strncpy(options->event, state.event, MAX_EVENT_LENGTH - 1);
    strncpy(options->event_data, state.event_data, MAX_EVENT_DATA_LENGTH - 1);
    strncpy(options->requested_playback_file, state.requested_playback_file, MAX_PATH_LENGTH - 1);

    // Load patient name if it exists and set patient path
    char patient_name[MAX_NAME_LENGTH];
    error = namespace_options_get_patient_name(options, patient_name, MAX_NAME_LENGTH);
    if (error == ERROR_NONE && patient_name[0] != '\0') {
        error = set_patient_path(options, patient_name);
    }

    return error;
}

ErrorCode namespace_options_set_root_data_dir(const char* drive_name) {
    char filepath[MAX_PATH_LENGTH];
    ErrorCode error = get_full_filepath(PATIENT_DRIVE_FILENAME, filepath, MAX_PATH_LENGTH);
    if (error != ERROR_NONE) return error;

    char root_dir[MAX_PATH_LENGTH];
    snprintf(root_dir, MAX_PATH_LENGTH, "%s\\data", drive_name);

    FILE* file = fopen(filepath, "wb");
    if (!file) return ERROR_FILE_OPEN;

    size_t len = strlen(root_dir);
    if (fwrite(root_dir, 1, len, file) != len) {
        fclose(file);
        return ERROR_FILE_WRITE;
    }

    fclose(file);
    return ERROR_NONE;
}

ErrorCode namespace_options_get_root_data_dir(char* dir, size_t max_length) {
    char filepath[MAX_PATH_LENGTH];
    ErrorCode error = get_full_filepath(PATIENT_DRIVE_FILENAME, filepath, MAX_PATH_LENGTH);
    if (error != ERROR_NONE) return error;

    FILE* file = fopen(filepath, "rb");
    if (!file) {
        // Default to ROOT_DATA_DIR if file doesn't exist
        strncpy(dir, ROOT_DATA_DIR, max_length - 1);
        return ERROR_NONE;
    }

    size_t read_size = fread(dir, 1, max_length - 1, file);
    dir[read_size] = '\0';
    
    fclose(file);
    return ERROR_NONE;
}

ErrorCode namespace_options_setup_watch(NamespaceOptions* options,
                                      OptionsDisplayCallback display_cb,
                                      NamespaceCallback namespace_cb,
                                      PatientNameCallback patient_cb) {
    if (!options) return ERROR_INVALID_PARAMETER;

    // Store callbacks
    options->options_display_callback = display_cb;
    options->namespace_callback = namespace_cb;
    options->patient_name_callback = patient_cb;

    // Setup directory watching
    wchar_t wide_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, K7_DATA_DIR, -1, wide_path, MAX_PATH);
    
    options->dir_handle = CreateFileW(
        wide_path,
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        NULL);
        
    if (options->dir_handle == INVALID_HANDLE_VALUE) {
        return ERROR_FILE_OPEN;
    }
    
    options->overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    options->should_run = TRUE;
    options->thread_handle = CreateThread(NULL, 0, watch_directory, options, 0, NULL);
    
    return ERROR_NONE;
}

ErrorCode namespace_options_setup_user_data_watch(NamespaceOptions* options,
                                                UserDataCallback callback) {
    if (!options || !options->patient_path[0]) return ERROR_INVALID_PARAMETER;

    options->user_data_callback = callback;

    // Create a new file handle for the patient directory
    wchar_t wide_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, options->patient_path, -1, wide_path, MAX_PATH);
    
    HANDLE patient_dir = CreateFileW(
        wide_path,
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        NULL);
        
    if (patient_dir == INVALID_HANDLE_VALUE) {
        return ERROR_FILE_OPEN;
    }

    // Start a new thread for watching patient directory
    HANDLE thread = CreateThread(NULL, 0, watch_directory, options, 0, NULL);
    if (!thread) {
        DWORD win_error = GetLastError();  // Get the Windows error code
        CloseHandle(patient_dir);
        log_error("Failed to create thread: Windows error %lu", win_error);
        return ERROR_INVALID_STATE;  // Use existing error code
    }

    return ERROR_NONE;
}

// Add new function to handle new user data records (similar to Python's __new_user_data_record)
static ErrorCode handle_new_user_data_record(NamespaceOptions* options, const char* user_record_path) {
    char* file_name = strrchr(user_record_path, '\\');
    if (!file_name) {
        file_name = (char*)user_record_path;
    } else {
        file_name++; // Skip the backslash
    }
    
    return namespace_options_set_event(options, EVENT_USER_RECORD_SAVED, file_name);
}