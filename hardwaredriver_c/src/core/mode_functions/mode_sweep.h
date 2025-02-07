#ifndef MODE_SWEEP_H
#define MODE_SWEEP_H

#include <stdio.h>
#include <windows.h>
#include <stdbool.h>
#include "src/core/mode_functions/mode_base.h"
#include "src/core/error_codes.h"
#include "src/core/utils/windows_queue.h"
#include "src/core/utils/namespace_options.h"

// Constants
#define SCALING_FACTOR 57.2958
#define MODE_SWEEP_READ_SIZE 1600
#define MODE_SWEEP_BLOCK_SIZE 16  // 8 bytes CMS + 8 bytes tilt
#define K7_MODE_TYPE_PATH "C:\\K7\\current_mode_type"

// Forward declaration for UI placeholder
typedef struct SweepDataUI SweepDataUI;

// Namespace option functions
ErrorCode set_current_patient_name(const char* patient_name);
ErrorCode toggle_recording(void);
ErrorCode start_cms_playback(const char* file_name);
ErrorCode start_playback(void);
ErrorCode mark_redraw_event(void);

// PatientNameWatcher
typedef struct {
    HANDLE dir_handle;
    HANDLE thread_handle;
    void (*callback)(const char*);
    bool running;
} PatientNameWatcher;

ErrorCode patient_name_watcher_create(PatientNameWatcher** watcher, void (*callback)(const char*));
void patient_name_watcher_destroy(PatientNameWatcher* watcher);

// ModeSweep class
typedef struct {
    ModeBase base;
    bool is_first_run;
    double front_angle;
    double side_angle;
    
    // Data processing
    DataQueue* sweep_queue;
    PatientNameWatcher* watcher;
    NamespaceOptions* namespace;
    
    // UI Integration (placeholder)
    SweepDataUI* plot_app;
    bool show_tilt_window;
    bool show_sweep_graph;
    bool tilt_enabled;
    HANDLE sweep_process;
    DataQueue* sweep_command_queue;
} ModeSweep;

// Constructor/Destructor
ErrorCode mode_sweep_create(ModeSweep** mode, SerialInterface* interface, 
                          ProcessManager* process_manager,
                          bool show_tilt_window, bool show_sweep_graph);
void mode_sweep_destroy(ModeSweep* mode);

// Core functionality
ErrorCode mode_sweep_compute_angle(double ref, double axis1, double axis2, double* result);
ErrorCode mode_sweep_compute_tilt_data(const uint8_t* tilt_values, double* front_angle, double* side_angle);
ErrorCode mode_sweep_process_data(ModeSweep* mode, const uint8_t* data, size_t length);

// UI Integration
ErrorCode mode_sweep_start(ModeSweep* mode);
ErrorCode mode_sweep_stop(ModeSweep* mode);
ErrorCode mode_sweep_save_mode_type(bool show_sweep_graph);

// Add new function declarations
ErrorCode mode_sweep_start_process(ModeSweep* mode);
ErrorCode mode_sweep_stop_process(ModeSweep* mode);
DWORD WINAPI sweep_process_function(LPVOID param);


/*
*STILL NEED TO PUT THE ACTUAL IMPLEMENTATION FOR SWEEP DATA UI REUSEABLE 
* --> Currently only a placeholder is present
*
*
*/



#endif // MODE_SWEEP_H