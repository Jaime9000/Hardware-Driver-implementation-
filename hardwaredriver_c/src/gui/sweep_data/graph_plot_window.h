#ifndef GRAPH_PLOT_WINDOW_H
#define GRAPH_PLOT_WINDOW_H

// System headers
#include <tcl.h>
#include <tk.h>
#include <plplot/plplot.h>

// Project headers
#include "src/gui/sweep_data/data_table.h"
#include "src/core/error_codes.h"
#include "src/gui/sweep_data/ui_classes/scrollable_frame.h"
#include "src/gui/sweep_data/create_ui.h"

// Constants
#define WINDOW_NAME "Graph Plot"
#define FONT_SIZE 6
#define MAX_DATA_POINTS 1000
/*we need to update this file to work with plplot stream and custom tk widget directly instead of using xwin */

/**
 * @brief Structure to hold plot data points
 */
typedef struct {
    PLFLT* x;              // X-axis values
    PLFLT* y;              // Y-axis values
    int count;             // Number of data points
} PlotData;

/**
 * @brief Structure to hold table data for printing
 */
typedef struct {
    char** dates;          // Array of date strings
    int* a_flex_values;    // Anterior flexion values
    int* p_ext_values;     // Posterior extension values
    int* r_flex_values;    // Right flexion values
    int* l_flex_values;    // Left flexion values
    size_t count;          // Number of rows
} TableData;

// Forward declaration
typedef struct GraphPlotWindow GraphPlotWindow;

// Constructor/Destructor
/**
 * @brief Creates a new graph plot window
 * 
 * @param interp Tcl interpreter
 * @param patient_path Path to patient data
 * @param scan_filter_type Type of scan filter
 * @param patient_name Name of patient
 * @return GraphPlotWindow* Pointer to new window, or NULL on failure
 */
GraphPlotWindow* graph_plot_window_create(Tcl_Interp* interp, 
                                        const char* patient_path,
                                        const char* scan_filter_type,
                                        const char* patient_name);

/**
 * @brief Destroys a graph plot window and frees resources
 * 
 * @param window Pointer to GraphPlotWindow instance
 */
void graph_plot_window_destroy(GraphPlotWindow* window);
// Window operations
/**
 * @brief Sets up the toolbar for the graph plot window
 * 
 * @param window Pointer to GraphPlotWindow instance
 * @return ErrorCode ERROR_NONE on success, error code otherwise
 */
ErrorCode graph_plot_window_setup_toolbar(GraphPlotWindow* window);

/**
 * @brief Prints the current graph and data table
 * 
 * @param window Pointer to GraphPlotWindow instance
 * @return ErrorCode ERROR_NONE on success, error code otherwise
 */
ErrorCode graph_plot_window_print(GraphPlotWindow* window);

/**
 * @brief Populates the graph with current data
 * 
 * @param window Pointer to GraphPlotWindow instance
 * @return ErrorCode ERROR_NONE on success, error code otherwise
 */
ErrorCode graph_plot_window_populate_data(GraphPlotWindow* window);

/**
 * @brief Gets the window path
 * 
 * @param window Pointer to GraphPlotWindow instance
 * @return const char* Window path, or NULL if window is NULL
 */
const char* graph_plot_window_get_path(const GraphPlotWindow* window);

#endif // GRAPH_PLOT_WINDOW_H