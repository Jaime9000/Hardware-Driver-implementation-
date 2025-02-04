#ifndef GRAPH_H
#define GRAPH_H

// System headers
#include <tcl.h>
#include <tk.h>
#include <stdbool.h>
#include <windows.h>

// Project headers
#include "src/core/error_codes.h"
#include "src/gui/sweep_data/ui_classes/data_class.h"
#include "src/gui/sweep_data/tilt_windows/picture_window_reusable.h"
#include "src/gui/sweep_data/tilt_windows/namespace_options.h"
#include "src/gui/sweep_data/ui_classes/graph_plot_window.h"
#include "src/gui/windows_api.h"
#include "src/utils/uuid.h"
#include "src/utils/serialize_deserialize.h"

// Constants
#define GAIN_VALUES_COUNT 4
#define SCAN_TYPE_VALUES_COUNT 3
#define SPEED_VALUES_COUNT 3
#define FILTER_COMBO_VALUES_COUNT 4
#define MAX_QUEUE_SIZE 1000
#define INSTRUCTIONS_TEXT_FILE_PATH "C:\\K7\\python\\sweep_instructions.txt"
#define MODE_TYPE_FILE_PATH "C:\\K7\\current_mode_type"
#define DEFAULT_RECORDING_TIME 16.0
#define DEFAULT_GAIN 45
#define DEFAULT_SPEED "1.0"
#define DEFAULT_SCAN_TYPE "A/P Pitch"

// Forward declaration of Graph struct
typedef struct Graph Graph;

// Callback function types
/**
 * Callback function for filter table operations
 * @param value The filter value to be processed
 * @return ErrorCode indicating success or failure
 */
typedef ErrorCode (*FilterTableCallback)(const char* value);

/**
 * Callback function for CMS operations
 * @param extra_filter Additional filter parameters
 * @param fast_replay Flag for fast replay mode
 * @param with_summary Flag to include summary
 * @return ErrorCode indicating success or failure
 */
typedef ErrorCode (*CmsCallback)(const char* extra_filter, bool fast_replay, bool with_summary);

// Constructor/Destructor
/**
 * Creates a new Graph instance
 * @param interp TCL interpreter instance
 * @param main_window Main Tk window
 * @param data_class Data class instance for managing data
 * @param filter_table_callback Callback for filter table operations
 * @param patient_path Path to patient data
 * @param patient_name Name of the patient
 * @param cms_callback Callback for CMS operations
 * @param namespace Namespace options instance
 * @return Pointer to new Graph instance or NULL on failure
 */
Graph* graph_create(Tcl_Interp* interp, 
                   Tk_Window main_window,
                   DataClass* data_class,
                   FilterTableCallback filter_table_callback,
                   const char* patient_path, 
                   const char* patient_name,
                   CmsCallback cms_callback, 
                   NamespaceOptions* namespace);

/**
 * Destroys a Graph instance and frees all associated resources
 * @param graph Pointer to Graph instance to destroy
 */
void graph_destroy(Graph* graph);

// Core functionality
/**
 * Starts the graph animation
 * @param graph Pointer to Graph instance
 * @param picture_windows_only Flag to show only picture windows
 * @param tilt_enabled Flag to enable tilt functionality
 * @return ErrorCode indicating success or failure
 */
ErrorCode graph_start(Graph* graph, bool picture_windows_only, bool tilt_enabled);

/**
 * Stops the graph animation
 * @param graph Pointer to Graph instance
 * @return ErrorCode indicating success or failure
 */
ErrorCode graph_stop(Graph* graph);

/**
 * Checks if the graph is currently running
 * @param graph Pointer to Graph instance
 * @return true if running, false otherwise
 */
bool graph_is_running(const Graph* graph);

/**
 * Registers TCL/TK commands for the graph
 * @param graph Pointer to Graph instance
 * @return ErrorCode indicating success or failure
 */
ErrorCode graph_register_commands(Graph* graph);

// Animation functions
/**
 * Performs one animation step for the graph
 * @param graph Pointer to Graph instance
 * @return ErrorCode indicating success or failure
 */
ErrorCode graph_animate(ClientData clientData);

/**
 * Performs one animation step for picture windows only
 * @param graph Pointer to Graph instance
 * @return ErrorCode indicating success or failure
 */
ErrorCode graph_animate_picture_window_only(ClientData clientData);

/**
 * Performs one animation step during playback
 * @param graph Pointer to Graph instance
 * @return ErrorCode indicating success or failure
 */
ErrorCode graph_animate_playback(Graph* graph);

// UI update functions
/**
 * Updates the patient name
 * @param graph Pointer to Graph instance
 * @param new_patient_name New name to set
 * @return ErrorCode indicating success or failure
 */
ErrorCode graph_update_patient_name(Graph* graph, const char* new_patient_name);

/**
 * Updates the patient path
 * @param graph Pointer to Graph instance
 * @param new_patient_path New path to set
 * @return ErrorCode indicating success or failure
 */
ErrorCode graph_update_patient_path(Graph* graph, const char* new_patient_path);

/**
 * Updates the plot data with new points
 * @param graph Pointer to Graph instance
 * @param y_points_frontal Frontal data points
 * @param y_points_sagittal Sagittal data points
 * @return ErrorCode indicating success or failure
 */
ErrorCode graph_update_plot_data(Graph* graph, 
                               const DataPoints* y_points_frontal, 
                               const DataPoints* y_points_sagittal);

// Property getters
/**
 * Gets the current gain label value
 * @param graph Pointer to Graph instance
 * @return Current gain label value
 */
double graph_get_gain_label(const Graph* graph);

/**
 * Gets the current scan type label
 * @param graph Pointer to Graph instance
 * @return Current scan type label
 */
const char* graph_get_scan_type_label(const Graph* graph);

/**
 * Gets the current speed label value
 * @param graph Pointer to Graph instance
 * @return Current speed label value
 */
double graph_get_speed_label(const Graph* graph);

/**
 * Gets the current scan filter type
 * @param graph Pointer to Graph instance
 * @return Current scan filter type
 */
const char* graph_get_scan_filter_type(const Graph* graph);

/**
 * Gets the current mode type from file
 * @return Current mode type value
 */
bool graph_get_mode_type(void);

// Internal setup functions
/**
 * Sets up combo box values
 * @param graph Pointer to Graph instance
 * @return ErrorCode indicating success or failure
 */
ErrorCode graph_setup_combo_values(Graph* graph);

/**
 * Sets up graph buttons
 * @param graph Pointer to Graph instance
 * @return ErrorCode indicating success or failure
 */
ErrorCode graph_setup_buttons(Graph* graph);

/**
 * Sets up graph with specified speed label
 * @param graph Pointer to Graph instance
 * @param speed_label Speed label to use
 * @return ErrorCode indicating success or failure
 */
ErrorCode graph_setup_graph(Graph* graph, const char* speed_label);

// Event handling
/**
 * Handles key press events
 * @param graph Pointer to Graph instance
 * @param keycode Code of the pressed key
 * @return ErrorCode indicating success or failure
 */
ErrorCode graph_handle_key_press(Graph* graph, int keycode);

/**
 * Shows instructions window
 * @param graph Pointer to Graph instance
 * @return ErrorCode indicating success or failure
 */
ErrorCode graph_show_instructions(Graph* graph);

// File operations
/**
 * Creates temporary data file
 * @param graph Pointer to Graph instance
 * @return ErrorCode indicating success or failure
 */
ErrorCode graph_create_temp_data(Graph* graph);

/**
 * Loads data from temporary file
 * @param graph Pointer to Graph instance
 * @return ErrorCode indicating success or failure
 */
ErrorCode graph_load_temp_data(Graph* graph);

// Graph plotting
/**
 * Updates the graph plot with current values
 * @param graph Pointer to Graph instance
 * @return ErrorCode indicating success or failure
 */
ErrorCode graph_plot_graph_values(Graph* graph);

#endif // GRAPH_H

/*
* Review this implementation (graph.h/c) to verify all functionality from python implementation is 
* present.
*
*/