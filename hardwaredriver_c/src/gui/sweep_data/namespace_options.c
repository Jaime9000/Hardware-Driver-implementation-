#include "namespace_options.h"
#include "serialize_deserialize.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <direct.h>

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

ErrorCode namespace_options_create(NamespaceOptions** options, bool reset_states) {
    *options = (NamespaceOptions*)calloc(1, sizeof(NamespaceOptions));
    if (!*options) return ERROR_MEMORY_ALLOCATION;
    
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
    char filepath[MAX_PATH_LENGTH];
    ErrorCode error = get_full_filepath(PATIENT_NAME_FILENAME, filepath, MAX_PATH_LENGTH);
    if (error != ERROR_NONE) return error;

    FILE* file = fopen(filepath, "wb");
    if (!file) return ERROR_FILE_OPEN;

    size_t name_len = strlen(name);
    if (fwrite(name, 1, name_len, file) != name_len) {
        fclose(file);
        return ERROR_FILE_WRITE;
    }

    fclose(file);
    return set_patient_path(options, name);
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