// In graph.h
typedef struct {
    double min_value;
    double max_value;
} MinMaxValues;

typedef struct {
    double* values;
    size_t count;
    size_t capacity;
    MinMaxValues range;  // Added range field
} PlotData;

// In graph.c
ErrorCode graph_update_plot_data(Graph* graph, 
                               const DataPoints* y_points_frontal, 
                               const DataPoints* y_points_sagittal) {
    if (!graph || !y_points_frontal || !y_points_sagittal) {
        return ERROR_INVALID_PARAMETER;
    }

    // Use PlotData structure from graph_plot_window.h
    PlotData frontal_data = {0};
    PlotData sagittal_data = {0};

    frontal_data.x = malloc(y_points_frontal->count * sizeof(PLFLT));
    frontal_data.y = malloc(y_points_frontal->count * sizeof(PLFLT));
    sagittal_data.x = malloc(y_points_sagittal->count * sizeof(PLFLT));
    sagittal_data.y = malloc(y_points_sagittal->count * sizeof(PLFLT));

    if (!frontal_data.x || !frontal_data.y || 
        !sagittal_data.x || !sagittal_data.y) {
        free(frontal_data.x);
        free(frontal_data.y);
        free(sagittal_data.x);
        free(sagittal_data.y);
        return ERROR_OUT_OF_MEMORY;
    }

    // Scale and transform data
    double gain_scale = graph->gain_label / 15.0;
    for (size_t i = 0; i < y_points_frontal->count; i++) {
        frontal_data.x[i] = (PLFLT)i;
        sagittal_data.x[i] = (PLFLT)i;
        
        frontal_data.y[i] = (PLFLT)(y_points_frontal->values[i] / gain_scale + 180.0);
        sagittal_data.y[i] = (PLFLT)(y_points_sagittal->values[i] / gain_scale);
    }

    frontal_data.count = y_points_frontal->count;
    sagittal_data.count = y_points_sagittal->count;

    // Update plot using PLplot functions
    plcol0(1);  // blue for frontal
    plline(frontal_data.count, frontal_data.x, frontal_data.y);
    
    plcol0(2);  // red for sagittal  
    plline(sagittal_data.count, sagittal_data.x, sagittal_data.y);

    // Cleanup
    free(frontal_data.x);
    free(frontal_data.y);
    free(sagittal_data.x);
    free(sagittal_data.y);

    return ERROR_NONE;
}


ErrorCode graph_plot_graph_values(Graph* graph) {
    if (!graph) return ERROR_INVALID_PARAMETER;

    // Create new graph plot window using existing implementation
    GraphPlotWindow* graph_window = graph_plot_window_create(
        graph->interp,
        graph->patient_path,
        graph->scan_filter_type,
        graph->patient_name
    );
    if (!graph_window) return ERROR_OUT_OF_MEMORY;

    // Set up window close handler first
    char close_cmd[512];
    snprintf(close_cmd, sizeof(close_cmd),
             "bind %s <Destroy> {"
             "    graph_window_close_callback %p %p"
             "}",
             graph_plot_window_get_path(graph_window),
             (void*)graph,
             (void*)graph_window);
    
    if (Tcl_Eval(graph->interp, close_cmd) != TCL_OK) {
        // Let the close callback handle cleanup
        Tcl_Eval(graph->interp, "destroy .graph_plot_window");
        return ERROR_TCL_EVAL;
    }

    // Hide existing windows using picture_window_reusable functions
    ErrorCode error = picture_window_reusable_hide_window(graph->frontal_window);
    if (error != ERROR_NONE) {
        Tcl_Eval(graph->interp, "destroy .graph_plot_window");
        return error;
    }

    error = picture_window_reusable_hide_window(graph->sagittal_window);
    if (error != ERROR_NONE) {
        Tcl_Eval(graph->interp, "destroy .graph_plot_window");
        return error;
    }

    // Hide main window
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "wm withdraw %s", graph->main_window_path);
    if (Tcl_Eval(graph->interp, cmd) != TCL_OK) {
        Tcl_Eval(graph->interp, "destroy .graph_plot_window");
        return ERROR_TCL_EVAL;
    }

    // Pause data capture
    data_class_pause_data_capture(graph->data_class);

    return ERROR_NONE;
}

// Callback function registered with Tcl
int graph_window_close_callback(ClientData client_data, Tcl_Interp* interp, 
                              int argc, const char* argv[]) {
    if (argc != 3) return TCL_ERROR;
    
    Graph* graph = (Graph*)strtoul(argv[1], NULL, 16);
    GraphPlotWindow* graph_window = (GraphPlotWindow*)strtoul(argv[2], NULL, 16);

    // Call filter table callback
    graph->filter_table_callback(NULL);

    // Show main windows using picture_window_reusable functions
    picture_window_reusable_show_window(graph->frontal_window);
    picture_window_reusable_show_window(graph->sagittal_window);

    // Show main window
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "wm deiconify %s", graph->main_window_path);
    Tcl_Eval(interp, cmd);

    // Resume data capture
    data_class_resume_data_capture(graph->data_class);

    // Cleanup graph window - only place where we destroy the window
    graph_plot_window_destroy(graph_window);

    return TCL_OK;
}