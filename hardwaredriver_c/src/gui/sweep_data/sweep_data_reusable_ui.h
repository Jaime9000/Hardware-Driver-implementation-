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
#include "data_table.h"
#include "data_class.h"
#include "graph.h"

#define DATA_VERSION "1.2"
#define MAX_PATH_LENGTH 1024

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
    Tk_Window main_window;
    DataQueue* data_queue;
    DataQueue* command_queue;
    NamespaceOptions* namespace;
    
    // UI Components
    DataTable* table;
    DataClass* data_class;
    Graph* graph;

    // Add nnuilder/UI components
    char* resource_path;
    
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

    //callbacks
    FilterTableCallback* table_filter_callback;
    CmsCallback* cms_callback;
    PlaybackCallback* playback_callback;    
    //cms_callback  
    
    // State
    ///bool running;
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