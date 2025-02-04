#ifndef NAMESPACE_OPTIONS_H
#define NAMESPACE_OPTIONS_H

// System headers
#include <stdbool.h>
#include <windows.h>
// Event types
// Project headers
#include "src/core/error_codes.h"

// Event type constants
#define EVENT_TOGGLE_RECORDING "toggle-recording"
#define EVENT_USER_RECORD_SAVED "user-record-saved"
#define EVENT_CMS_RECORDING_PLAYBACK "cms-playback"
#define EVENT_CMS_START_PLAYBACK "playback"
#define EVENT_MARK_REDRAW_TOOL "mark-redraw-tool"

// Directory constants
#define K7_DATA_DIR "C:\\K7"
#define ROOT_DATA_DIR "C:\\data"

// Buffer size constants
#define MAX_PATH_LENGTH 1024
#define MAX_EVENT_LENGTH 64
#define MAX_EVENT_DATA_LENGTH 256
#define MAX_NAME_LENGTH 256

// Forward declarations for callback types
///NOT SURE THAT WE NEED THESE 
typedef void (*OptionsDisplayCallback)(bool show_windows);
typedef void (*NamespaceCallback)(void);
typedef void (*PatientNameCallback)(const char* filename);
typedef void (*UserDataCallback)(const char* filename);

/**
 * @brief Structure to hold namespace options and state
 */
typedef struct {
    // Patient information
    char patient_name[MAX_NAME_LENGTH];     // Full patient name
    char first_name[MAX_NAME_LENGTH];       // Patient's first name
    char last_name[MAX_NAME_LENGTH];        // Patient's last name
    char patient_path[MAX_PATH_LENGTH];     // Path to patient data
    
    // Event handling
    char requested_playback_file[MAX_PATH_LENGTH];  // File requested for playback
    char event[MAX_EVENT_LENGTH];           // Current event type
    char event_data[MAX_EVENT_DATA_LENGTH]; // Event-specific data
    
    // State flags
    bool exit_thread;                       // Thread exit flag
    bool app_ready;                         // Application ready state
   // bool options_display;                   // Display options state
    
    // Directory (file) watching
    HANDLE dir_handle;                      // Directory handle
    HANDLE thread_handle;                   // Watch thread handle
    OVERLAPPED overlapped;                  // Async I/O structure
    volatile BOOL should_run;               // Thread control flag
    
    // Callbacks
    //display options
    bool options_display;
    OptionsDisplayCallback options_display_callback;  // Display options callback
    NamespaceCallback namespace_callback;            // Namespace event callback
    PatientNameCallback patient_name_callback;       // Patient name change callback
    UserDataCallback user_data_callback;             // User data update callback
} NamespaceOptions;
// Constructor/destructor
/**
 * @brief Creates a new namespace options instance
 * 
 * @param options Pointer to store the created options
 * @param reset_states Whether to reset state values
 * @return ErrorCode ERROR_NONE on success, error code otherwise
 */
ErrorCode namespace_options_create(NamespaceOptions** options, bool reset_states);

/**
 * @brief Destroys a namespace options instance and frees resources
 * 
 * @param options Pointer to NamespaceOptions instance
 */
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