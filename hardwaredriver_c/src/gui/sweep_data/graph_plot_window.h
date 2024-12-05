#ifndef GRAPH_PLOT_WINDOW_H
#define GRAPH_PLOT_WINDOW_H

#include <tcl.h>
#include <tk.h>
#include <plplot/plplot.h>
#include "data_table.h"
#include "error_codes.h"

#define WINDOW_NAME "Graph Plot"
#define FONT_SIZE 6
#define MAX_DATA_POINTS 1000

typedef struct {
    PLFLT* x;
    PLFLT* y;
    int count;
} PlotData;

typedef struct GraphPlotWindow GraphPlotWindow;

// Constructor/Destructor
GraphPlotWindow* graph_plot_window_create(Tcl_Interp* interp, 
                                        const char* patient_path,
                                        const char* scan_filter_type,
                                        const char* patient_name);
void graph_plot_window_destroy(GraphPlotWindow* window);

// Window operations
ErrorCode graph_plot_window_setup(GraphPlotWindow* window);
ErrorCode graph_plot_window_print(GraphPlotWindow* window);
ErrorCode graph_plot_window_populate_data(GraphPlotWindow* window);

#endif // GRAPH_PLOT_WINDOW_H