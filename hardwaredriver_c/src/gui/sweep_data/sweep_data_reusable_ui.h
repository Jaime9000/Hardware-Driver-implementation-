#ifndef SWEEP_DATA_UI_REUSABLE_H
#define SWEEP_DATA_UI_REUSABLE_H

// System headers
#include <windows.h>
#include <shlwapi.h>
#include <tcl.h>
#include <tk.h>

// Project headers
#include "src/gui/sweep_data/windows_queue.h"
#include "src/gui/sweep_data/namespace_options.h"
#include "src/core/error_codes.h"
#include "src/utils/serialize_deserialize.h"
#include "src/gui/sweep_data/image_helpers.h"
#include "src/gui/sweep_data/ui_classes/scrollable_frame.h"
#include "src/gui/sweep_data/data_table.h"
#include "src/gui/sweep_data/ui_classes/data_class.h"
#include "src/gui/sweep_data/graph.h"

// Constants
#define DATA_VERSION "1.2"
#define MAX_PATH_LENGTH 1024

/**
 * @brief Recording modes for the application
 */
typedef enum {
    RECORDING_MODE_NONE,        // No recording active
    RECORDING_MODE_CMS_SCAN,    // CMS scan recording mode
    RECORDING_MODE_NORMAL_SCAN  // Normal scan recording mode
} RecordingMode;

// Forward declarations for callback types
typedef ErrorCode (*FilterTableCallback)(void* app, const char* filter_type);
typedef ErrorCode (*CmsCallback)(void* app, const char* extra_filter, bool with_summary, bool fast_replay);
typedef ErrorCode (*PlaybackCallback)(void* app, const char* filename);

/**
 * @brief Main application structure for the reusable plot UI
 */
typedef struct {
    // TCL/TK Components
    Tcl_Interp* interp;              // Tcl interpreter
    ScrollableFrame* scrollable_frame;// Scrollable frame widget
    
    // Windows Components
    Tk_Window main_window;           // Main Tk window
    DataQueue* data_queue;           // Queue for data processing
    DataQueue* command_queue;        // Queue for commands
    NamespaceOptions* namespace;     // Namespace options
    
    // UI Components
    DataTable* table;                // Data table widget
    DataClass* data_class;           // Data class instance
    Graph* graph;                    // Graph widget
    
    // Add nnuilder/UI components
    char* resource_path

    // Path information
    char patient_path[MAX_PATH_LENGTH];    // Path to patient data
    char resource_path[MAX_PATH_LENGTH];   // Path to resources
    
    // UI Elements (Tcl/Tk widgets)
    char* a_flex_label;              // Anterior flexion label
    char* p_ext_label;               // Posterior extension label
    char* r_flex_label;              // Right flexion label
    char* l_flex_label;              // Left flexion label
    char* ap_pitch_label;            // A/P pitch label
    char* lateral_roll_label;        // Lateral roll label

    // Callbacks
    FilterTableCallback* table_filter_callback;
    CmsCallback* cms_callback;
    PlaybackCallback* playback_callback;    
    //cms_callback  
    
    // State
      ///bool running;
    bool picture_windows_only;       // Show only picture windows
    bool tilt_enabled;               // Tilt functionality enabled
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

/**
 * Sets up the data table component
 * @param app Pointer to PlotAppReusable instance
 * @return ErrorCode indicating success or failure
 */
ErrorCode plot_app_reusable_setup_table(PlotAppReusable* app);

/**
 * Sets up value labels with images
 * @param app Pointer to PlotAppReusable instance
 * @return ErrorCode indicating success or failure
 */
ErrorCode plot_app_reusable_setup_value_labels(PlotAppReusable* app);


// Path management
ErrorCode plot_app_reusable_change_patient_path(PlotAppReusable* app, const char* new_path);

/**
 * Handles playback of recorded data
 * @param app Pointer to PlotAppReusable instance
 * @param file_name Name of file to playback
 * @return ErrorCode indicating success or failure
 */
ErrorCode plot_app_reusable_playback_callback(PlotAppReusable* app, const char* file_name);

/**
 * Handles CMS window playback
 * @param app Pointer to PlotAppReusable instance
 * @param extra_filter Additional filter parameters
 * @param with_summary Include summary flag
 * @param fast_replay Fast replay flag
 * @return ErrorCode indicating success or failure
 */
ErrorCode plot_app_reusable_playback_cms_window(PlotAppReusable* app, 
                                              const char* extra_filter,
                                              bool with_summary,
                                              bool fast_replay);

/**
 * Main event processing loop
 * @param app Pointer to PlotAppReusable instance
 * @return ErrorCode indicating success or failure
 */
ErrorCode plot_app_reusable_process_events(PlotAppReusable* app);

#endif // SWEEP_DATA_UI_REUSABLE_H