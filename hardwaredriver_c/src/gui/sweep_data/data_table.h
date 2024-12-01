#ifndef DATA_TABLE_H
#define DATA_TABLE_H

#include <tcl.h>
#include <tk.h>
#include <stdbool.h>
#include <windows.h>
#include "error_codes.h"

#define RECORDING_PHASE_1 "first_phase"
#define RECORDING_PHASE_2 "second_phase"
#define RECORDING_PHASE_READABLE_1 "First Run"
#define RECORDING_PHASE_READABLE_2 "Second Run"
#define SCROLLABLE_CLASS_NAME "scrollable"
#define CLICKABLE_CLASS_NAME "clickable"

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
void data_table_repopulate(DataTable* table, const char* scan_filter_type);
int* data_table_get_checked_values(DataTable* table, size_t* count);

#endif // DATA_TABLE_H