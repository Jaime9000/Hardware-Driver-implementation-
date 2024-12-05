#ifndef GRAPH_H
#define GRAPH_H

#include <tcl.h>
#include <tk.h>
#include <stdbool.h>
#include <time.h>
#include "data_class.h"
#include "picture_window_reusable.h"
#include "namespace_options.h"
#include "error_codes.h"
#include "builder.h"

#define GAIN_VALUES_COUNT 4
#define SCAN_TYPE_VALUES_COUNT 3
#define SPEED_VALUES_COUNT 3
#define FILTER_COMBO_VALUES_COUNT 4
#define MAX_QUEUE_SIZE 1000

typedef struct {
    double* values;
    size_t count;
    size_t capacity;
} PlotData;

typedef struct Graph Graph;
typedef ErrorCode (*FilterTableCallback)(const char* value);
typedef ErrorCode (*CmsCallback)(const char* extra_filter, bool with_summary, bool fast_replay);

// Constructor/Destructor
Graph* graph_create(Tcl_Interp* interp,
                   Tk_Window main_window,
                   DataClass* data_class,
                   Builder* builder,
                   FilterTableCallback filter_table_callback,
                   const char* patient_path,
                   const char* patient_name,
                   CmsCallback cms_callback,
                   NamespaceOptions* namespace);

void graph_destroy(Graph* graph);

// Graph operations
ErrorCode graph_start(Graph* graph, bool picture_windows_only, bool tilt_enabled);
ErrorCode graph_stop(Graph* graph);
ErrorCode graph_update_patient_path(Graph* graph, const char* new_patient_path);
ErrorCode graph_update_patient_name(Graph* graph, const char* new_patient_name);

// Animation and update functions
ErrorCode graph_animate(Graph* graph);
ErrorCode graph_animate_picture_window_only(Graph* graph);
ErrorCode graph_animate_playback(Graph* graph);
ErrorCode graph_update_plot_data(Graph* graph, const DataPoints* y_points_frontal, const DataPoints* y_points_sagittal);

// Property getters/setters
double graph_get_gain_label(const Graph* graph);
const char* graph_get_scan_type_label(const Graph* graph);
double graph_get_speed_label(const Graph* graph);
const char* graph_get_scan_filter_type(const Graph* graph);
bool graph_get_mode_type(void);

// Internal setup functions
ErrorCode graph_setup_combo_values(Graph* graph);
ErrorCode graph_setup_buttons(Graph* graph);
ErrorCode graph_setup_graph(Graph* graph, const char* speed_label);

#endif // GRAPH_H