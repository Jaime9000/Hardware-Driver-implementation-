#ifndef NAMESPACE_OPTIONS_H
#define NAMESPACE_OPTIONS_H

#include <stdbool.h>
#include "error_codes.h"

// Event types
#define EVENT_TOGGLE_RECORDING "toggle-recording"
#define EVENT_USER_RECORD_SAVED "user-record-saved"
#define EVENT_CMS_RECORDING_PLAYBACK "cms-playback"
#define EVENT_CMS_START_PLAYBACK "playback"
#define EVENT_MARK_REDRAW_TOOL "mark-redraw-tool"

#define K7_DATA_DIR "C:\\K7"
#define ROOT_DATA_DIR "C:\\data"
#define MAX_PATH_LENGTH 1024
#define MAX_EVENT_LENGTH 64
#define MAX_EVENT_DATA_LENGTH 256
#define MAX_NAME_LENGTH 256

typedef struct {
    char patient_name[MAX_NAME_LENGTH];
    char first_name[MAX_NAME_LENGTH];
    char last_name[MAX_NAME_LENGTH];
    char patient_path[MAX_PATH_LENGTH];
    char requested_playback_file[MAX_PATH_LENGTH];
    char event[MAX_EVENT_LENGTH];
    char event_data[MAX_EVENT_DATA_LENGTH];
    bool exit_thread;
    bool app_ready;
    
    // File watching
    HANDLE dir_handle;
    HANDLE thread_handle;
    OVERLAPPED overlapped;
    volatile BOOL should_run;
    
    // Callbacks
    OptionsDisplayCallback options_display_callback;
    NamespaceCallback namespace_callback;
    PatientNameCallback patient_name_callback;
    UserDataCallback user_data_callback;
    
    // Display options
    bool options_display;
} NamespaceOptions;

// Constructor/destructor
ErrorCode namespace_options_create(NamespaceOptions** options, bool reset_states);
void namespace_options_destroy(NamespaceOptions* options);

// Property getters/setters
ErrorCode namespace_options_get_patient_name(NamespaceOptions* options, char* name, size_t max_length);
ErrorCode namespace_options_set_patient_name(NamespaceOptions* options, const char* name);

ErrorCode namespace_options_get_event(NamespaceOptions* options, char* event, size_t max_event_length, 
                                    char* event_data, size_t max_data_length);
ErrorCode namespace_options_set_event(NamespaceOptions* options, const char* event, const char* event_data);

// File operations
ErrorCode namespace_options_save_to_directory(const NamespaceOptions* options);
ErrorCode namespace_options_load_from_directory(NamespaceOptions* options);

// Utility functions
ErrorCode namespace_options_set_root_data_dir(const char* drive_name);
ErrorCode namespace_options_get_root_data_dir(char* dir, size_t max_length);

// Add these functions
ErrorCode namespace_options_setup_watch(NamespaceOptions* options,
                                      OptionsDisplayCallback display_cb,
                                      NamespaceCallback namespace_cb,
                                      PatientNameCallback patient_cb);
ErrorCode namespace_options_setup_user_data_watch(NamespaceOptions* options,
                                                UserDataCallback callback);

// Add to header file
ErrorCode namespace_options_reset_on_first_init(void);

#endif // NAMESPACE_OPTIONS_H