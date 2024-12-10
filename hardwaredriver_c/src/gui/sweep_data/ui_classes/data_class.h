#ifndef DATA_CLASS_H
#define DATA_CLASS_H

#include <tcl.h>
#include <tk.h>
#include <stdbool.h>
#include <math.h>
#include "error_codes.h"
#include "image_window_options.h"
#include "windows_queue.h"
#include "serialize_deserialize.h"
#include <stdio.h>
#include "cJSON.h"
#include <windows.h>

// Make RecordingModes strings accessible
extern const char* RECORDING_MODE_STRINGS[];

// Recording modes enum (matches Python's RecordingModes)
typedef enum {
    A_P_PITCH,
    LAT_ROLL,
    OTHER,
    CMS_SCAN,
    RECORDING_MODE_COUNT  // Used to iterate over modes
} RecordingMode;

// Make RecordingStates strings accessible
extern const char* RECORDING_STATE_STRINGS[];

// Recording states enum (matches Python's RecordingStates)
typedef enum {
    RECORDING_ON,
    RECORDING_COMPLETE,
    NOT_RECORDING,
    PLAYBACK,
    PLAYBACK_COMPLETE,
    PLAYBACK_PAUSED,
    ERROR_NOT_RECORDING,
    RECORDING_STATE_COUNT  // Used to iterate over states
} RecordingState;

// Make calculate_min_max_values accessible to other files
typedef struct {
    double max_value;
    double min_value;
} MinMaxValues;

ErrorCode calculate_min_max_values(const double* values, 
                                 size_t count, 
                                 MinMaxValues* result);

// Make DataPoints structure public for other files to use
typedef struct {
    double* values;
    double* timestamps;  // Seconds from start of recording
    size_t count;
    size_t capacity;
} DataPoints;

// Make SavedData structure public
typedef struct {
    DataPoints frontal_points;
    DataPoints sagittal_points;
    char* scan_type;
} SavedData;

// Forward declaration
typedef struct DataClass DataClass;

// Constructor/Destructor
DataClass* data_class_create(Tcl_Interp* interp, 
                           const char* builder_path,
                           const char* namespace_path,
                           DataQueue* data_queue);
void data_class_destroy(DataClass* data);

// Recording control
ErrorCode data_class_start_recording(DataClass* data, const char* scan_type_label);
ErrorCode data_class_stop_recording(DataClass* data);
ErrorCode data_class_toggle_recording(DataClass* data, const char* scan_type_label);
ErrorCode data_class_pause_data_capture(DataClass* data);
ErrorCode data_class_resume_data_capture(DataClass* data);
ErrorCode data_class_clear_all(DataClass* data, bool clear_all);

// Data management
ErrorCode data_class_append_data(DataClass* data, 
                               const double* y_points, 
                               size_t points_count);
ErrorCode data_class_save_recording(DataClass* data, 
                                  const char* patient_path, 
                                  const char* extra_filter);
ErrorCode data_class_set_image_window_values(DataClass* data, 
                                           double frontal, 
                                           double sagittal);

/*
 * Note on Python's __empty_y_points:
 * The Python version includes an __empty_y_points method that stores current points
 * in a __saved_data_points list before reinitializing. However, analysis showed that
 * __saved_data_points is never used in the codebase:
 * 1. Points are stored but never retrieved
 * 2. List is cleared in clear_all() before potential use
 * 3. Actual data saving uses __saved_data, not __saved_data_points
 * 
 * Therefore, this C implementation only includes reinitialize_y_points without the
 * historical points storage. If __saved_data_points functionality is needed later,
 * it can be added to match Python's implementation.
 */
ErrorCode data_class_reinitialize_y_points(DataClass* data);

// Playback control
ErrorCode data_class_start_playback(DataClass* data, const char* filename);
ErrorCode data_class_stop_playback(DataClass* data);
ErrorCode data_class_pause_playback(DataClass* data);
ErrorCode data_class_resume_playback(DataClass* data);
ErrorCode data_class_set_playback_speed(DataClass* data, double speed);
ErrorCode data_class_mark_playback_complete(DataClass* data);

// UI control
ErrorCode data_class_set_button_state(DataClass* data, 
                                    const char* button_name, 
                                    bool active);

// State getters
bool data_class_is_recording(const DataClass* data);
bool data_class_is_playback(const DataClass* data);
bool data_class_is_paused(const DataClass* data);
SYSTEMTIME data_class_get_recording_start_time(const DataClass* data);
const SavedData* data_class_get_saved_data(const DataClass* data);
const DataPoints* data_class_get_frontal_points(const DataClass* data);
const DataPoints* data_class_get_sagittal_points(const DataClass* data);
double data_class_get_playback_speed(const DataClass* data);
const char* data_class_get_scan_type(const DataClass* data);

// Add these function declarations
ErrorCode data_class_load_playback_speed(const char* config_path, double* speed);
ErrorCode data_class_save_playback_speed(const char* config_path, double speed);

#endif // DATA_CLASS_H