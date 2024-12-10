#include "graph_plot_window.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <windows.h>
#include <math.h>
#include "printer.h"
#include "logger.h"

#define WINDOW_NAME "Graph Plot"  // Global constant for display title

struct GraphPlotWindow {
    Tcl_Interp* interp;
    const char* window_path;  // Will store the actual widget path
    char* patient_path;
    char* scan_filter_type;
    char* patient_name;
    DataTable* table;
    ScrollableFrame* scrollable_frame;
    
    // PLplot data
    PlotData a_flex;
    PlotData p_ext;
    PlotData r_flex;
    PlotData l_flex;
    char** dates;
    int date_count;
    
    // Table data for printing
    TableData table_data;
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

static void init_table_data(TableData* data) {
    memset(data, 0, sizeof(TableData));
}

static void free_table_data(TableData* data) {
    if (!data) return;
    
    for (size_t i = 0; i < data->count; i++) {
        free(data->dates[i]);
    }
    free(data->dates);
    free(data->a_flex_values);
    free(data->p_ext_values);
    free(data->r_flex_values);
    free(data->l_flex_values);
    memset(data, 0, sizeof(TableData));
}

static void convert_date_format(const char* input_date, char* output_date, size_t output_size) {
    int year, month, day;
    sscanf(input_date, "%d-%d-%d", &year, &month, &day);
    snprintf(output_date, output_size, "%02d-%02d-%04d", month, day, year);
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
    window->window_path = ".graph_plot_window";  // Store actual widget path

    // Format patient name like Python version
    char* formatted_name = strdup(patient_name);
    for (char* p = formatted_name; *p; ++p) {
        *p = (*p == '+') ? ' ' : *p;
        *p = (p == formatted_name || *(p-1) == ' ') ? toupper(*p) : tolower(*p);
    }
    window->patient_name = formatted_name;

    // Initialize plot data structures
    init_plot_data(&window->a_flex);
    init_plot_data(&window->p_ext);
    init_plot_data(&window->r_flex);
    init_plot_data(&window->l_flex);

    // Create UI using create_ui (similar to Python's builder.add_from_file)
    create_ui(interp);

    // Get reference to graph_plot_window element and set its title
    char cmd[256];
    snprintf(cmd, sizeof(cmd), 
             "set window_ref [%s]; "  // Use stored path
             "$window_ref configure -title {%s}",
             window->window_path, WINDOW_NAME);
    if (Tcl_Eval(interp, cmd) != TCL_OK) {
        graph_plot_window_destroy(window);
        return NULL;
    }

    // Create scrollable frame in the container (similar to Python's create_scrollable_frame)
    window->scrollable_frame = scrollable_frame_create(
        interp, 
        ".graph_plot_window.frame1.table_data_frame_container_2"
    );
    if (!window->scrollable_frame) {
        graph_plot_window_destroy(window);
        return NULL;
    }

    // Initialize table (similar to Python's Table initialization)
    window->table = data_table_create(
        interp, 
        patient_path, 
        NULL,  // playback_callback 
        0, 0,  // column, row
        graph_plot_window_populate_data  // on_check callback
    );
    if (!window->table) {
        graph_plot_window_destroy(window);
        return NULL;
    }

    // Populate initial data (similar to Python's table.re_populate_data)
    data_table_repopulate(window->table, scan_filter_type);

    // Setup toolbar and initial plot
    graph_plot_window_setup_toolbar(window);
    graph_plot_window_populate_data(window);

    return window;
}

ErrorCode graph_plot_window_setup_toolbar(GraphPlotWindow* window) {
    if (!window) return ERROR_INVALID_PARAMETER;

    // Configure print button (similar to Python's print_button.configure)
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "bind .graph_plot_window.frame1.frame3.print_button <Button-1> {"
             "    graph_plot_window_print"
             "}");
    
    return Tcl_Eval(window->interp, cmd) == TCL_OK ? ERROR_NONE : ERROR_TCL_EVAL;
}

ErrorCode graph_plot_window_populate_data(GraphPlotWindow* window) {
    if (!window) return ERROR_INVALID_PARAMETER;

    // Get checked rows
    size_t row_count;
    const TableRow* rows = data_table_get_checked_rows(window->table, &row_count);
    if (!rows) return ERROR_NO_DATA;

    // Update table data for printing
    free_table_data(&window->table_data);
    window->table_data.count = row_count;
    window->table_data.dates = (char**)malloc(row_count * sizeof(char*));
    window->table_data.a_flex_values = (int*)malloc(row_count * sizeof(int));
    window->table_data.p_ext_values = (int*)malloc(row_count * sizeof(int));
    window->table_data.r_flex_values = (int*)malloc(row_count * sizeof(int));
    window->table_data.l_flex_values = (int*)malloc(row_count * sizeof(int));

    // Setup plot canvas
    plsdev("xwin");
    plsetopt("geometry", "700x400");
    plinit();

    // Match Python's fig.subplots_adjust()
    plsdiori(0.03, 0.05, 0.98, 0.95);  // left, bottom, right, top margins

    // Configure the canvas widget
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             ".graph_plot_window.frame1.graph_plot_canvas configure "
             "-width 700 -height 400");
    Tcl_Eval(window->interp, cmd);

    // Get current date using Windows API
    SYSTEMTIME st;
    GetLocalTime(&st);
    char date_str[32];
    snprintf(date_str, sizeof(date_str), "%02d-%02d-%04d", 
             st.wMonth, st.wDay, st.wYear);

    // Set titles matching Python format
    pllab("Date", "Value", "");  // Set axis labels
    pltimefmt("%m-%d-%Y");       // Set date format for x-axis

    char title[256];
    snprintf(title, sizeof(title), "Scan Type: %s", window->scan_filter_type);
    plmtex("t", 2.0, 0.5, 0.5, title);  // Center title

    snprintf(title, sizeof(title), "Patient: %s", window->patient_name);
    plmtex("t", 2.0, 0.0, 0.0, title);  // Left title

    snprintf(title, sizeof(title), "Graphed on: %s", date_str);
    plmtex("t", 2.0, 1.0, 1.0, title);  // Right title

    // Sort rows by date
    qsort(rows, row_count, sizeof(TableRow), compare_rows_by_date);

    // Store dates for x-axis labels and populate plot data
    window->dates = (char**)realloc(window->dates, row_count * sizeof(char*));
    window->date_count = row_count;
    
    // Clear previous data
    window->a_flex.count = 0;
    window->p_ext.count = 0;
    window->r_flex.count = 0;
    window->l_flex.count = 0;

    // Populate data points
    for (size_t i = 0; i < row_count; i++) {
        char formatted_date[32];
        convert_date_format(rows[i].date, formatted_date, sizeof(formatted_date));
        window->dates[i] = strdup(formatted_date);
        window->table_data.dates[i] = strdup(formatted_date);
        
        // Set plot data
        window->a_flex.x[i] = (PLFLT)i;
        window->p_ext.x[i] = (PLFLT)i;
        window->r_flex.x[i] = (PLFLT)i;
        window->l_flex.x[i] = (PLFLT)i;
        
        // Store both plot and table data
        window->a_flex.y[i] = (PLFLT)((int)rows[i].a_flex);
        window->p_ext.y[i] = (PLFLT)((int)fabs(rows[i].p_ext));
        window->r_flex.y[i] = (PLFLT)((int)rows[i].r_flex);
        window->l_flex.y[i] = (PLFLT)((int)fabs(rows[i].l_flex));
        
        window->table_data.a_flex_values[i] = (int)rows[i].a_flex;
        window->table_data.p_ext_values[i] = (int)fabs(rows[i].p_ext);
        window->table_data.r_flex_values[i] = (int)rows[i].r_flex;
        window->table_data.l_flex_values[i] = (int)fabs(rows[i].l_flex);
        
        window->a_flex.count++;
        window->p_ext.count++;
        window->r_flex.count++;
        window->l_flex.count++;
    }

    // Setup plot environment
    plenv(0, row_count - 1, 0, 90, 0, 0);  // Match Python's ax.set_ylim([0, 90])
    pllab("Date", "Value", "");  // Add labels

    // Set x-axis ticks and labels
    PLFLT ticks[row_count];
    for (int i = 0; i < row_count; i++) {
        ticks[i] = i;
    }
    plsticks(ticks, window->dates, row_count);

    // Plot all data series with matching Python colors
    // A Flex (Blue)
    plcol0(1);  // blue
    plpoin(window->a_flex.count, window->a_flex.x, window->a_flex.y, 1);
    plline(window->a_flex.count, window->a_flex.x, window->a_flex.y);

    // P Ext (Red)
    plcol0(2);  // red
    plpoin(window->p_ext.count, window->p_ext.x, window->p_ext.y, 1);
    plline(window->p_ext.count, window->p_ext.x, window->p_ext.y);

    // R Flex (Black)
    plcol0(15);  // black
    plpoin(window->r_flex.count, window->r_flex.x, window->r_flex.y, 1);
    plline(window->r_flex.count, window->r_flex.x, window->r_flex.y);

    // L Flex (Yellow)
    plcol0(7);  // yellow
    plpoin(window->l_flex.count, window->l_flex.x, window->l_flex.y, 1);
    plline(window->l_flex.count, window->l_flex.x, window->l_flex.y);

    // Add legend
    const char *legends[] = {"A Flex", "P Ext", "R Flex", "L Flex"};
    PLINT opt_array[] = {PL_LEGEND_LINE | PL_LEGEND_SYMBOL};
    PLINT text_colors[] = {1, 2, 15, 7};  // Match plot colors
    PLINT line_colors[] = {1, 2, 15, 7};
    PLINT line_styles[] = {1, 1, 1, 1};
    PLINT symbol_numbers[] = {1, 1, 1, 1};
    PLINT symbol_colors[] = {1, 2, 15, 7};
    PLFLT line_widths[] = {1.0, 1.0, 1.0, 1.0};
    PLFLT symbol_scales[] = {1.0, 1.0, 1.0, 1.0};
    
    pllegend(&opt_array[0], 
             PL_POSITION_RIGHT | PL_POSITION_TOP,
             0.1,  // x offset
             0.1,  // y offset
             0.1,  // plot_width
             0,    // bg_color
             0,    // bb_color
             1,    // bb_style
             4,    // nrow
             0,    // ncolumn
             4,    // nlegend
             text_colors,
             (const char *const *)legends,
             line_colors,
             line_styles,
             line_widths,
             symbol_colors,
             symbol_numbers,
             symbol_scales);

    plfont(1);  // Set font family
    plschr(0.0, FONT_SIZE/10.0);  // Set font size (PLplot uses different scale)

    // After configuring canvas in populate_data():
    snprintf(cmd, sizeof(cmd),
             "%s.frame1.graph_plot_canvas grid -column 0 -row 2 -rowspan 20",
             window->window_path);
    Tcl_Eval(window->interp, cmd);

    return ERROR_NONE;
}

ErrorCode graph_plot_window_print(GraphPlotWindow* window) {
    if (!window) return ERROR_INVALID_PARAMETER;

    char temp_filename[] = "temp_XXXXXX.png";
    mkstemp(temp_filename);

    // Save current device
    char old_dev[32];
    plgdev(old_dev);

    // Switch to PNG device with larger size for table
    plsdev("pngcairo");
    plsfnam(temp_filename);
    plsetopt("geometry", "1000x800");
    
    // Recreate plot
    graph_plot_window_populate_data(window);
    
    if (window->table_data.count > 0) {
        // Add table below plot
        PLFLT xmin = 0.0;
        PLFLT ymin = -0.45;
        PLFLT width = 1.0;
        PLFLT height = 0.28;
        
        // Draw table with cell text and column labels
        plenv(xmin, xmin + width, ymin, ymin + height, 0, 0);
        
        // Add column headers
        const char* headers[] = {"Dates", "A Flex", "P Ext", "R Flex", "L Flex"};
        for (int i = 0; i < 5; i++) {
            plptex(xmin + (i + 0.5) * width/5, ymin + height - 0.02, 
                   1.0, 0.0, 0.5, headers[i]);
        }
        
        // Add data rows
        for (size_t i = 0; i < window->table_data.count; i++) {
            PLFLT y = ymin + height - 0.05 * (i + 1);
            
            plptex(xmin + width/10, y, 1.0, 0.0, 0.5, window->table_data.dates[i]);
            
            char value[32];
            snprintf(value, sizeof(value), "%d", window->table_data.a_flex_values[i]);
            plptex(xmin + 3*width/10, y, 1.0, 0.0, 0.5, value);
            
            snprintf(value, sizeof(value), "%d", window->table_data.p_ext_values[i]);
            plptex(xmin + 5*width/10, y, 1.0, 0.0, 0.5, value);
            
            snprintf(value, sizeof(value), "%d", window->table_data.r_flex_values[i]);
            plptex(xmin + 7*width/10, y, 1.0, 0.0, 0.5, value);
            
            snprintf(value, sizeof(value), "%d", window->table_data.l_flex_values[i]);
            plptex(xmin + 9*width/10, y, 1.0, 0.0, 0.5, value);
        }
    }
    
    plend1();
    plsdev(old_dev);
    
    return open_print_dialog(temp_filename);
}

void graph_plot_window_destroy(GraphPlotWindow* window) {
    if (!window) return;

    // First destroy all plot data
    free_plot_data(&window->a_flex);
    free_plot_data(&window->p_ext);
    free_plot_data(&window->r_flex);
    free_plot_data(&window->l_flex);
    free_table_data(&window->table_data);

    // Clean up dates array
    for (int i = 0; i < window->date_count; i++) {
        free(window->dates[i]);
    }
    free(window->dates);

    // Destroy all child widgets
    char cmd[256];
    snprintf(cmd, sizeof(cmd), 
             "foreach child [winfo children %s] { destroy $child }",
             window->window_path);
    Tcl_Eval(window->interp, cmd);

    // Quit the window (matching Python's quit call)
    snprintf(cmd, sizeof(cmd), "%s quit", window->window_path);
    Tcl_Eval(window->interp, cmd);

    // Destroy the window itself
    snprintf(cmd, sizeof(cmd), "destroy %s", window->window_path);
    Tcl_Eval(window->interp, cmd);

    // Clean up other resources
    free(window->patient_path);
    free(window->scan_filter_type);
    free(window->patient_name);

    if (window->scrollable_frame) {
        scrollable_frame_destroy(window->scrollable_frame);
    }

    if (window->table) {
        data_table_destroy(window->table);
    }

    plend();
    free(window);
}

// Add window property getter like Python's @property window
const char* graph_plot_window_get_path(const GraphPlotWindow* window) {
    return window->window_path;
}