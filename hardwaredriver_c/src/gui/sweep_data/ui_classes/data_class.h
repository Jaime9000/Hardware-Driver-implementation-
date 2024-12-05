#ifndef DATA_CLASS_H
#define DATA_CLASS_H

#include <tcl.h>
#include <tk.h>
#include <stdbool.h>
#include <time.h>
#include "error_codes.h"
#include "image_window_options.h"
#include "windows_queue.h"

// Recording modes enum (matches Python's RecordingModes)
typedef enum {
    A_P_PITCH,
    LAT_ROLL,
    OTHER,
    CMS_SCAN
} RecordingMode;

// Recording states enum (matches Python's RecordingStates)
typedef enum {
    RECORDING_ON,
    RECORDING_COMPLETE,
    NOT_RECORDING,
    PLAYBACK,
    PLAYBACK_COMPLETE,
    PLAYBACK_PAUSED,
    ERROR_NOT_RECORDING
} RecordingState;

// Data structures
typedef struct {
    double* values;
    double* timestamps;
    size_t count;
    size_t capacity;
} DataPoints;

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
ErrorCode data_class_reinitialize_y_points(DataClass* data);

// Playback control
ErrorCode data_class_start_playback(DataClass* data, const char* filename);
ErrorCode data_class_stop_playback(DataClass* data);
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
time_t data_class_get_recording_start_time(const DataClass* data);
const SavedData* data_class_get_saved_data(const DataClass* data);
const DataPoints* data_class_get_frontal_points(const DataClass* data);
const DataPoints* data_class_get_sagittal_points(const DataClass* data);
double data_class_get_playback_speed(const DataClass* data);
const char* data_class_get_scan_type(const DataClass* data);

#endif // DATA_CLASS_H