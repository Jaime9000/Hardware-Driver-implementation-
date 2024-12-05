#include "graph_plot_window.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "printer.h"
#include "logger.h"

struct GraphPlotWindow {
    Tcl_Interp* interp;
    char* patient_path;
    char* scan_filter_type;
    char* patient_name;
    DataTable* table;
    char* window_path;
    
    // PLplot data
    PlotData a_flex;
    PlotData p_ext;
    PlotData r_flex;
    PlotData l_flex;
    char** dates;
    int date_count;
};

static void init_plot_data(PlotData* data) {
    data->x = (PLFLT*)calloc(MAX_DATA_POINTS, sizeof(PLFLT));
    data->y = (PLFLT*)calloc(MAX_DATA_POINTS, sizeof(PLFLT));
    data->count = 0;
}

static void free_plot_data(PlotData* data) {
    free(data->x);
    free(data->y);
}

GraphPlotWindow* graph_plot_window_create(Tcl_Interp* interp, 
                                        const char* patient_path,
                                        const char* scan_filter_type,
                                        const char* patient_name) {
    GraphPlotWindow* window = calloc(1, sizeof(GraphPlotWindow));
    if (!window) return NULL;

    window->interp = interp;
    window->patient_path = strdup(patient_path);
    window->scan_filter_type = strdup(scan_filter_type);
    window->patient_name = strdup(patient_name);

    // Initialize plot data structures
    init_plot_data(&window->a_flex);
    init_plot_data(&window->p_ext);
    init_plot_data(&window->r_flex);
    init_plot_data(&window->l_flex);
    
    // Create main window using Tcl/Tk
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "toplevel .graph_plot_window; "
             "wm title .graph_plot_window {%s}", WINDOW_NAME);
    if (Tcl_Eval(interp, cmd) != TCL_OK) {
        graph_plot_window_destroy(window);
        return NULL;
    }

    window->window_path = strdup(".graph_plot_window");
    
    // Initialize PLplot
    plsdev("tkwin");
    plinit();
    
    return window;
}

ErrorCode graph_plot_window_populate_data(GraphPlotWindow* window) {
    if (!window) return ERROR_INVALID_PARAMETER;

    // Get data from table
    TableData* data = data_table_get_data(window->table, window->scan_filter_type);
    if (!data) return ERROR_NO_DATA;

    // Process data into plot format
    for (int i = 0; i < data->row_count; i++) {
        window->a_flex.x[i] = i;
        window->a_flex.y[i] = data->rows[i].a_flex;
        window->p_ext.x[i] = i;
        window->p_ext.y[i] = abs(data->rows[i].p_ext);
        window->r_flex.x[i] = i;
        window->r_flex.y[i] = data->rows[i].r_flex;
        window->l_flex.x[i] = i;
        window->l_flex.y[i] = abs(data->rows[i].l_flex);
    }
    window->a_flex.count = data->row_count;
    window->p_ext.count = data->row_count;
    window->r_flex.count = data->row_count;
    window->l_flex.count = data->row_count;

    // Set up plot
    plswin(0, data->row_count, 0, 90);
    plcol0(15);  // White background
    plenv(0, data->row_count, 0, 90, 0, 0);

    // Plot titles
    pllab("Date", "Value", window->scan_filter_type);
    char title[256];
    time_t now = time(NULL);
    strftime(title, sizeof(title), "Graphed on: %Y-%m-%d", localtime(&now));
    plmtex("t", 2.0, 1.0, 1.0, title);
    plmtex("t", 2.0, 0.0, 0.0, window->patient_name);

    // Plot data series
    // A Flex (Blue)
    plcol0(1);
    plline(window->a_flex.count, window->a_flex.x, window->a_flex.y);
    plpoin(window->a_flex.count, window->a_flex.x, window->a_flex.y, 1);

    // P Ext (Red)
    plcol0(2);
    plline(window->p_ext.count, window->p_ext.x, window->p_ext.y);
    plpoin(window->p_ext.count, window->p_ext.x, window->p_ext.y, 1);

    // R Flex (Black)
    plcol0(15);
    plline(window->r_flex.count, window->r_flex.x, window->r_flex.y);
    plpoin(window->r_flex.count, window->r_flex.x, window->r_flex.y, 1);

    // L Flex (Yellow)
    plcol0(3);
    plline(window->l_flex.count, window->l_flex.x, window->l_flex.y);
    plpoin(window->l_flex.count, window->l_flex.x, window->l_flex.y, 1);

    // Add legend
    plcol0(15);
    pllsty(1);
    pllegend(PL_LEGEND_BACKGROUND | PL_LEGEND_BOUNDING_BOX, 0,
             0.1, 0.9, 0.1,
             15, 1, 1, 0, 0,
             4, window->a_flex.count,
             (const char*[]){"A Flex", "P Ext", "R Flex", "L Flex"},
             (PLINT[]){1, 2, 15, 3},
             (PLINT[]){1, 1, 1, 1},
             (PLINT[]){1, 1, 1, 1},
             (PLFLT[]){1., 1., 1., 1.},
             NULL, NULL, NULL, NULL);

    return ERROR_NONE;
}

ErrorCode graph_plot_window_print(GraphPlotWindow* window) {
    if (!window) return ERROR_INVALID_PARAMETER;

    // Save current device
    char old_dev[32];
    plgdev(old_dev);

    // Switch to PNG device
    plsdev("pngcairo");
    plsfnam("temp_plot.png");
    
    // Recreate plot for PNG
    graph_plot_window_populate_data(window);
    
    // Close PNG device
    plend1();

    // Switch back to original device
    plsdev(old_dev);
    
    // Open print dialog
    return open_print_dialog("temp_plot.png");
}

void graph_plot_window_destroy(GraphPlotWindow* window) {
    if (!window) return;

    free_plot_data(&window->a_flex);
    free_plot_data(&window->p_ext);
    free_plot_data(&window->r_flex);
    free_plot_data(&window->l_flex);

    free(window->patient_path);
    free(window->scan_filter_type);
    free(window->patient_name);
    free(window->window_path);

    plend();
    free(window);
}