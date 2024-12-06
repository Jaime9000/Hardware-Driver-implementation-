#ifndef SWEEP_DATA_UI_REUSABLE_H
#define SWEEP_DATA_UI_REUSABLE_H

#include <windows.h>
#include <shlwapi.h>
#include <tcl.h>
#include <tk.h>
#include "windows_queue.h"
#include "namespace_options.h"
#include "error_codes.h"
#include "serialize_deserialize.h"
#include "image_helpers.h"
#include "scrollable_frame.h"

#define DATA_VERSION "1.2"
#define MAX_PATH_LENGTH 1024

// Forward declarations
typedef struct DataTable DataTable;
typedef struct DataClass DataClass;
typedef struct Graph Graph;

typedef enum {
    RECORDING_MODE_NONE,
    RECORDING_MODE_CMS_SCAN,
    RECORDING_MODE_NORMAL_SCAN
} RecordingMode;

typedef struct {
    // TCL/TK Components
    Tcl_Interp* interp;
    ScrollableFrame* scrollable_frame;
    
    // Windows Components
    HWND main_window;
    DataQueue* data_queue;
    DataQueue* command_queue;
    NamespaceOptions* namespace;
    
    // UI Components
    DataTable* table;
    DataClass* data_class;
    Graph* graph;
    
    // Path information
    char patient_path[MAX_PATH_LENGTH];
    char resource_path[MAX_PATH_LENGTH];  // For images and resources
    
    // UI Elements (Tcl/Tk widgets)
    char* a_flex_label;
    char* p_ext_label;
    char* r_flex_label;
    char* l_flex_label;
    char* ap_pitch_label;
    char* lateral_roll_label;
    
    // State
    bool running;
    bool picture_windows_only;
    bool tilt_enabled;
} PlotAppReusable;

// Constructor/Destructor
ErrorCode plot_app_reusable_create(PlotAppReusable** app, 
                                 DataQueue* data_queue,
                                 NamespaceOptions* namespace,
                                 DataQueue* command_queue);
void plot_app_reusable_destroy(PlotAppReusable* app);

// Core functionality
ErrorCode plot_app_reusable_run(PlotAppReusable* app);
ErrorCode plot_app_reusable_stop(PlotAppReusable* app);
ErrorCode plot_app_reusable_setup_table(PlotAppReusable* app);
ErrorCode plot_app_reusable_setup_value_labels(PlotAppReusable* app);

// Playback functions
ErrorCode plot_app_reusable_playback_callback(PlotAppReusable* app, const char* file_name);
ErrorCode plot_app_reusable_playback_cms_window(PlotAppReusable* app, 
                                              const char* extra_filter,
                                              bool with_summary,
                                              bool fast_replay);

// Path management
ErrorCode plot_app_reusable_change_patient_path(PlotAppReusable* app, const char* new_path);

#endif // SWEEP_DATA_UI_REUSABLE_H