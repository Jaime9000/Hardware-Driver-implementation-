#ifndef DATA_TABLE_H
#define DATA_TABLE_H

#include <tcl.h>
#include <tk.h>
#include <stdbool.h>
#include <windows.h>
#include "error_codes.h"
#include "serialize_deserialize.h"
#include "ui_classes/data_class.h"

#define RECORDING_PHASE_1 "first_phase"
#define RECORDING_PHASE_2 "second_phase"
#define RECORDING_PHASE_READABLE_1 "First Run"
#define RECORDING_PHASE_READABLE_2 "Second Run"

// Column indices for the table
enum TableColumns {
    COL_CHECKBOX = 0,
    COL_NUMBER,
    COL_DATETIME,
    COL_A_FLEX,
    COL_P_EXT,
    COL_R_FLEX,
    COL_L_FLEX,
    COL_COUNT
};

typedef struct {
    char* filename;
    char* datetime;
    double max_frontal;
    double min_frontal;
    double max_sagittal;
    double min_sagittal;
    int data_index;
    bool is_checked;
} TableRow;

typedef struct DataTable DataTable;
typedef void (*PlaybackCallback)(const char* file_name);
typedef void (*CheckCallback)(void);

// Core table functions
ErrorCode data_table_create(DataTable** table, 
                          Tcl_Interp* interp,
                          const char* patient_path_data,
                          PlaybackCallback playback_callback,
                          int column,
                          int row,
                          CheckCallback on_check);

void data_table_destroy(DataTable* table);

// Data management
ErrorCode data_table_repopulate(DataTable* table, const char* scan_filter_type);
const TableRow* data_table_get_checked_rows(const DataTable* table, size_t* count);
ErrorCode data_table_set_row_checked(DataTable* table, size_t row_index, bool checked);

// UI Event handlers
ErrorCode data_table_handle_click(DataTable* table, const char* filename);
ErrorCode data_table_handle_scroll(DataTable* table, int delta);

#endif // DATA_TABLE_H