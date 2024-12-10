#ifndef GRAPH_PLOT_WINDOW_H
#define GRAPH_PLOT_WINDOW_H

#include <tcl.h>
#include <tk.h>
#include <plplot/plplot.h>
#include "data_table.h"
#include "error_codes.h"
#include "ui_classes/scrollable_frame.h"
#include "create_ui.h"

#define WINDOW_NAME "Graph Plot"
#define FONT_SIZE 6
#define MAX_DATA_POINTS 1000

typedef struct {
    PLFLT* x;
    PLFLT* y;
    int count;
} PlotData;

typedef struct {
    char** dates;
    int* a_flex_values;
    int* p_ext_values;
    int* r_flex_values;
    int* l_flex_values;
    size_t count;
} TableData;

typedef struct GraphPlotWindow GraphPlotWindow;

// Constructor/Destructor
GraphPlotWindow* graph_plot_window_create(Tcl_Interp* interp, 
                                        const char* patient_path,
                                        const char* scan_filter_type,
                                        const char* patient_name);
void graph_plot_window_destroy(GraphPlotWindow* window);

// Window operations
ErrorCode graph_plot_window_setup_toolbar(GraphPlotWindow* window);
ErrorCode graph_plot_window_print(GraphPlotWindow* window);
ErrorCode graph_plot_window_populate_data(GraphPlotWindow* window);

#endif // GRAPH_PLOT_WINDOW_H