#ifndef DATA_TABLE_H
#define DATA_TABLE_H

// System headers
#include <tcl.h>
#include <tk.h>
#include <stdbool.h>
#include <windows.h>

// Project headers
#include "src/core/error_codes.h"
#include "src/utils/serialize_deserialize.h"
#include "src/gui/sweep_data/ui_classes/data_class.h"

// Constants
#define RECORDING_PHASE_1 "first_phase"
#define RECORDING_PHASE_2 "second_phase"
#define RECORDING_PHASE_READABLE_1 "First Run"
#define RECORDING_PHASE_READABLE_2 "Second Run"

/**
 * @brief Column indices for the table
 */
enum TableColumns {
    COL_CHECKBOX = 0,    // Checkbox column for selection
    COL_NUMBER,          // Row number
    COL_DATETIME,        // Date and time
    COL_A_FLEX,          // Anterior flexion
    COL_P_EXT,          // Posterior extension
    COL_R_FLEX,          // Right flexion
    COL_L_FLEX,          // Left flexion
    COL_COUNT            // Total number of columns
};

/**
 * @brief Structure representing a row in the data table
 */
typedef struct {
    char* filename;          // Name of the data file
    char* datetime;          // Formatted date/time string
    double max_frontal;      // Maximum frontal plane value
    double min_frontal;      // Minimum frontal plane value
    double max_sagittal;     // Maximum sagittal plane value
    double min_sagittal;     // Minimum sagittal plane value
    int data_index;          // Index for data identification
    bool is_checked;         // Selection state
} TableRow;

// Forward declarations
typedef struct DataTable DataTable;
typedef void (*PlaybackCallback)(const char* file_name);
typedef void (*CheckCallback)(void);

/**
 * @brief Creates a new data table instance
 * 
 * @param table Pointer to store the created table
 * @param interp Tcl interpreter
 * @param patient_path_data Path to patient data directory
 * @param playback_callback Callback for playback events
 * @param column Grid column position
 * @param row Grid row position
 * @param on_check Callback for checkbox events
 * @return ErrorCode ERROR_NONE on success, error code otherwise
 */
ErrorCode data_table_create(DataTable** table, 
                          Tcl_Interp* interp,
                          const char* patient_path_data,
                          PlaybackCallback playback_callback,
                          int column,
                          int row,
                          CheckCallback on_check);

/**
 * @brief Destroys a data table instance and frees resources
 * 
 * @param table Pointer to DataTable instance
 */
void data_table_destroy(DataTable* table);

// Data management functions
ErrorCode data_table_repopulate(DataTable* table, const char* scan_filter_type);
const TableRow* data_table_get_checked_rows(const DataTable* table, size_t* count);
ErrorCode data_table_set_row_checked(DataTable* table, size_t row_index, bool checked);

// UI Event handlers
ErrorCode data_table_handle_click(DataTable* table, const char* filename);
ErrorCode data_table_handle_scroll(DataTable* table, int delta);

#endif // DATA_TABLE_H