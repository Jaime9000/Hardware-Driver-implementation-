#include "data_table.h"
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "utils.h"
#include "logger.h"
#include "ui_classes/commons.h"

#define BUFFER_SIZE 4096
#define MAX_ROWS 1000
#define DEFAULT_COLUMN_WIDTH 10
#define DATETIME_COLUMN_WIDTH 20
#define NUMBER_COLUMN_WIDTH 3

struct DataTable {
    Tcl_Interp* interp;
    char* patient_path_data;
    PlaybackCallback playback_callback;
    CheckCallback on_check;
    int column;
    int row;
    
    // Directory watching
    HANDLE dir_handle;
    HANDLE thread_handle;
    OVERLAPPED overlapped;
    char buffer[BUFFER_SIZE];
    volatile BOOL should_run;
    
    // Table data
    TableRow* rows;
    size_t row_count;
    size_t row_capacity;
    char* frame_path;
    
    // Scroll state
    int scroll_offset;
    int visible_rows;
};

static void free_table_row(TableRow* row) {
    if (row) {
        free(row->filename);
        free(row->datetime);
    }
}

static ErrorCode create_table_widgets(DataTable* table) {
    char cmd[512];
    
    snprintf(cmd, sizeof(cmd), "destroy .table_frame");
    if (Tcl_Eval(table->interp, cmd) != TCL_OK) return ERROR_TCL_EVAL;
    
    snprintf(cmd, sizeof(cmd), "frame .table_frame");
    if (Tcl_Eval(table->interp, cmd) != TCL_OK) return ERROR_TCL_EVAL;
    
    snprintf(cmd, sizeof(cmd), "grid .table_frame -column %d -row %d", 
             table->column, table->row);
    if (Tcl_Eval(table->interp, cmd) != TCL_OK) return ERROR_TCL_EVAL;
    
    table->frame_path = strdup(".table_frame");
    return ERROR_NONE;
}

static DWORD WINAPI watch_directory(LPVOID param) {
    DataTable* table = (DataTable*)param;
    DWORD bytes_returned;
    
    while (table->should_run) {
        if (ReadDirectoryChangesW(
                table->dir_handle,
                table->buffer,
                BUFFER_SIZE,
                TRUE,
                FILE_NOTIFY_CHANGE_LAST_WRITE,
                &bytes_returned,
                &table->overlapped,
                NULL)) {
            
            WaitForSingleObject(table->overlapped.hEvent, INFINITE);
            
            // Queue repopulate event to Tcl interpreter thread
            Tcl_Eval(table->interp, "after idle {RepopulateTable}");
        }
    }
    return 0;
}

static ErrorCode init_directory_watching(DataTable* table) {
    wchar_t wide_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, table->patient_path_data, -1, wide_path, MAX_PATH);
    
    table->dir_handle = CreateFileW(
        wide_path,
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        NULL);
        
    if (table->dir_handle == INVALID_HANDLE_VALUE) return ERROR_FILE_OPEN;
    
    table->overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    table->thread_handle = CreateThread(NULL, 0, watch_directory, table, 0, NULL);
    
    return ERROR_NONE;
}

static ErrorCode load_sweep_data(const char* filepath, TableRow* row) {
    SweepData* data = NULL;
    ErrorCode error = sweep_data_deserialize(filepath, &data);
    if (error != ERROR_NONE) return error;

    MinMaxValues sagittal_values, frontal_values;
    error = calculate_min_max_values(data->sagittal.values, data->sagittal.count, &sagittal_values);
    if (error != ERROR_NONE) {
        sweep_data_free(data);
        return error;
    }

    error = calculate_min_max_values(data->frontal.values, data->frontal.count, &frontal_values);
    if (error != ERROR_NONE) {
        sweep_data_free(data);
        return error;
    }

    row->max_sagittal = sagittal_values.max_value;
    row->min_sagittal = sagittal_values.min_value;
    row->max_frontal = frontal_values.max_value;
    row->min_frontal = frontal_values.min_value;

    // Determine data index based on run type
    if (strstr(data->run_type, RECORDING_PHASE_READABLE_1)) {
        row->data_index = 0;
    } else {
        row->data_index = 1;
    }

    sweep_data_free(data);
    return ERROR_NONE;
}

static ErrorCode add_table_row(DataTable* table, const char* filename) {
    if (table->row_count >= table->row_capacity) {
        size_t new_capacity = table->row_capacity * 2;
        TableRow* new_rows = realloc(table->rows, new_capacity * sizeof(TableRow));
        if (!new_rows) return ERROR_MEMORY_ALLOCATION;
        
        table->rows = new_rows;
        table->row_capacity = new_capacity;
    }

    TableRow* row = &table->rows[table->row_count];
    memset(row, 0, sizeof(TableRow));

    char filepath[PATH_MAX];
    snprintf(filepath, sizeof(filepath), "%s/%s", table->patient_path_data, filename);

    // Parse datetime from filename using utils.h function
    SYSTEMTIME decoded_time;
    ErrorCode error = decode_encoded_datetime(filename, &decoded_time);
    if (error != ERROR_NONE) {
        log_error("Failed to decode datetime from filename: %s", filename);
        return error;
    }

    char datetime_str[32];
    snprintf(datetime_str, sizeof(datetime_str), "%02d-%02d-%04d",
             decoded_time.wMonth, decoded_time.wDay, decoded_time.wYear);

    row->filename = strdup(filename);
    row->datetime = strdup(datetime_str);
    row->is_checked = false;

    error = load_sweep_data(filepath, row);
    if (error != ERROR_NONE) {
        free_table_row(row);
        return error;
    }

    table->row_count++;
    return ERROR_NONE;
}

static int compare_rows_by_datetime(const void* a, const void* b) {
    const TableRow* row_a = (const TableRow*)a;
    const TableRow* row_b = (const TableRow*)b;
    return strcmp(row_a->datetime, row_b->datetime);
}

static ErrorCode load_table_data(DataTable* table, const char* scan_filter_type) {
    DIR* dir = opendir(table->patient_path_data);
    if (!dir) return ERROR_FILE_OPEN;

    // Clear existing rows
    for (size_t i = 0; i < table->row_count; i++) {
        free_table_row(&table->rows[i]);
    }
    table->row_count = 0;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_REG) continue;

        char filepath[PATH_MAX];
        snprintf(filepath, sizeof(filepath), "%s/%s", 
                table->patient_path_data, entry->d_name);

        // Check file type and filter conditions
        SweepData* data = NULL;
        if (sweep_data_deserialize(filepath, &data) == ERROR_NONE) {
            // Skip CMS scan files and apply filter
            if (data->run_type == CMS_SCAN || 
                (scan_filter_type && scan_filter_type[0] != '\0' && 
                 strcmp(data->run_type, scan_filter_type) != 0 &&
                 strcmp(scan_filter_type, "None") != 0)) {
                sweep_data_free(data);
                continue;
            }
            sweep_data_free(data);
        } else {
            continue;  // Skip files that can't be deserialized
        }

        ErrorCode error = add_table_row(table, entry->d_name);
        if (error != ERROR_NONE) {
            log_error("Failed to add table row for %s: %d", entry->d_name, error);
            continue;
        }
    }

    closedir(dir);

    // Sort rows by datetime
    if (table->row_count > 0) {
        qsort(table->rows, table->row_count, sizeof(TableRow), compare_rows_by_datetime);
    }

    return ERROR_NONE;
}

static int get_column_width(int column) {
    switch (column) {
        case COL_NUMBER:
            return NUMBER_COLUMN_WIDTH;
        case COL_DATETIME:
            return DATETIME_COLUMN_WIDTH;
        default:
            return DEFAULT_COLUMN_WIDTH;
    }
}

static ErrorCode create_table_headers(DataTable* table) {
    char cmd[512];
    const char* headers[] = {"#", "Date Time", "A Flex", "P Ext", "R Flex", "L Flex"};
    int start_col = table->on_check ? 1 : 0;

    if (table->on_check) {
        snprintf(cmd, sizeof(cmd),
                "entry %s.h0 -width %d -fg black -font {Arial 9};"
                "%s.h0 insert end {Checked};"
                "grid %s.h0 -row 0 -column 0",
                table->frame_path, DEFAULT_COLUMN_WIDTH,
                table->frame_path,
                table->frame_path);
        if (Tcl_Eval(table->interp, cmd) != TCL_OK) return ERROR_TCL_EVAL;
    }

    for (int i = 0; i < COL_COUNT - 1; i++) {
        const char* color = "black";
        if (strstr("A Flex P Ext", headers[i])) color = "blue";
        if (strstr("R Flex L Flex", headers[i])) color = "red";

        snprintf(cmd, sizeof(cmd),
                "entry %s.h%d -width %d -fg %s -font {Arial 9 bold};"
                "%s.h%d insert end {%s};"
                "grid %s.h%d -row 0 -column %d;"
                "bindtags %s.h%d {%s}",
                table->frame_path, i + start_col, get_column_width(i),
                color, table->frame_path, i + start_col, headers[i],
                table->frame_path, i + start_col, i + start_col,
                table->frame_path, i + start_col, SCROLLABLE_CLASS_NAME);
        
        if (Tcl_Eval(table->interp, cmd) != TCL_OK) return ERROR_TCL_EVAL;
    }

    return ERROR_NONE;
}

static ErrorCode create_table_row(DataTable* table, size_t row_idx, size_t display_idx) {
    char cmd[1024];
    TableRow* row = &table->rows[row_idx];
    int start_col = table->on_check ? 1 : 0;
    int display_row = display_idx + 1;  // +1 for header row

    if (table->on_check) {
        snprintf(cmd, sizeof(cmd),
                "checkbutton %s.cb%zu -variable cb%zu -command {CheckHandler %zu};"
                "grid %s.cb%zu -row %d -column 0",
                table->frame_path, row_idx, row_idx, row_idx,
                table->frame_path, row_idx, display_row);
        if (Tcl_Eval(table->interp, cmd) != TCL_OK) return ERROR_TCL_EVAL;
    }

    // Create cells
    double values[] = {
        (double)(row_idx + 1),
        0.0,  // datetime is string
        fabs(row->max_sagittal),
        fabs(row->min_sagittal),
        fabs(row->max_frontal),
        fabs(row->min_frontal)
    };

    for (int col = 0; col < COL_COUNT - 1; col++) {
        const char* text;
        char value_str[32];
        
        if (col == COL_DATETIME - 1) {
            text = row->datetime;
        } else {
            snprintf(value_str, sizeof(value_str), "%.2f", values[col]);
            text = value_str;
        }

        snprintf(cmd, sizeof(cmd),
                "entry %s.c%zu_%d -width %d -fg black -font {Arial 9};"
                "%s.c%zu_%d insert end {%s};"
                "grid %s.c%zu_%d -row %d -column %d;"
                "bindtags %s.c%zu_%d {%s %s %s}",
                table->frame_path, row_idx, col, get_column_width(col),
                table->frame_path, row_idx, col, text,
                table->frame_path, row_idx, col, display_row, col + start_col,
                table->frame_path, row_idx, col,
                SCROLLABLE_CLASS_NAME, CLICKABLE_CLASS_NAME, row->filename);

        if (Tcl_Eval(table->interp, cmd) != TCL_OK) return ERROR_TCL_EVAL;
    }

    return ERROR_NONE;
}

static ErrorCode update_table_display(DataTable* table) {
    // Clear existing widgets except headers
    char cmd[256];
    snprintf(cmd, sizeof(cmd), 
             "foreach w [grid slaves %s] {"
             "    if {[string match *.h* $w] == 0} {"
             "        destroy $w"
             "    }"
             "}", table->frame_path);
    if (Tcl_Eval(table->interp, cmd) != TCL_OK) return ERROR_TCL_EVAL;

    // Calculate visible range
    size_t start_idx = table->scroll_offset;
    size_t end_idx = start_idx + table->visible_rows;
    if (end_idx > table->row_count) end_idx = table->row_count;

    // Create visible rows
    for (size_t i = start_idx; i < end_idx; i++) {
        ErrorCode error = create_table_row(table, i, i - start_idx);
        if (error != ERROR_NONE) return error;
    }

    return ERROR_NONE;
}

ErrorCode data_table_create(DataTable** table, 
                          Tcl_Interp* interp,
                          const char* patient_path_data,
                          PlaybackCallback playback_callback,
                          int column,
                          int row,
                          CheckCallback on_check) {
    if (!table || !interp || !patient_path_data) return ERROR_INVALID_PARAMETER;

    DataTable* new_table = calloc(1, sizeof(DataTable));
    if (!new_table) return ERROR_MEMORY_ALLOCATION;

    new_table->interp = interp;
    new_table->patient_path_data = strdup(patient_path_data);
    new_table->playback_callback = playback_callback;
    new_table->on_check = on_check;
    new_table->column = column;
    new_table->row = row;
    new_table->should_run = TRUE;
    new_table->visible_rows = 20;  // Default visible rows
    new_table->row_capacity = MAX_ROWS;
    
    new_table->rows = calloc(MAX_ROWS, sizeof(TableRow));
    if (!new_table->rows) {
        data_table_destroy(new_table);
        return ERROR_MEMORY_ALLOCATION;
    }

    ErrorCode error = create_table_widgets(new_table);
    if (error != ERROR_NONE) {
        data_table_destroy(new_table);
        return error;
    }

    error = create_table_headers(new_table);
    if (error != ERROR_NONE) {
        data_table_destroy(new_table);
        return error;
    }

    error = init_directory_watching(new_table);
    if (error != ERROR_NONE) {
        data_table_destroy(new_table);
        return error;
    }

    error = load_table_data(new_table, NULL);
    if (error != ERROR_NONE) {
        data_table_destroy(new_table);
        return error;
    }

    error = update_table_display(new_table);
    if (error != ERROR_NONE) {
        data_table_destroy(new_table);
        return error;
    }

    *table = new_table;
    return ERROR_NONE;
}

void data_table_destroy(DataTable* table) {
    if (!table) return;

    table->should_run = FALSE;
    
    if (table->thread_handle) {
        CancelIoEx(table->dir_handle, &table->overlapped);
        WaitForSingleObject(table->thread_handle, INFINITE);
        CloseHandle(table->thread_handle);
        CloseHandle(table->overlapped.hEvent);
        CloseHandle(table->dir_handle);
    }

    if (table->rows) {
        for (size_t i = 0; i < table->row_count; i++) {
            free_table_row(&table->rows[i]);
        }
        free(table->rows);
    }

    free(table->patient_path_data);
    free(table->frame_path);
    free(table);
}

ErrorCode data_table_repopulate(DataTable* table, const char* scan_filter_type) {
    if (!table) return ERROR_INVALID_PARAMETER;

    ErrorCode error = load_table_data(table, scan_filter_type);
    if (error != ERROR_NONE) return error;

    table->scroll_offset = 0;  // Reset scroll position
    return update_table_display(table);
}

ErrorCode data_table_handle_click(DataTable* table, const char* filename) {
    if (!table || !filename) return ERROR_INVALID_PARAMETER;
    
    if (table->playback_callback) {
        table->playback_callback(filename);
    }
    return ERROR_NONE;
}

ErrorCode data_table_handle_scroll(DataTable* table, int delta) {
    if (!table) return ERROR_INVALID_PARAMETER;

    int new_offset = table->scroll_offset - (delta / 120);  // Windows wheel delta
    
    if (new_offset < 0) {
        new_offset = 0;
    }
    
    int max_offset = (int)table->row_count - table->visible_rows;
    if (max_offset < 0) max_offset = 0;
    
    if (new_offset > max_offset) {
        new_offset = max_offset;
    }

    if (new_offset != table->scroll_offset) {
        table->scroll_offset = new_offset;
        return update_table_display(table);
    }

    return ERROR_NONE;
}

const TableRow* data_table_get_checked_rows(const DataTable* table, size_t* count) {
    if (!table || !count) return NULL;
    *count = table->row_count;
    return table->rows;
}

ErrorCode data_table_set_row_checked(DataTable* table, size_t row_index, bool checked) {
    if (!table || row_index >= table->row_count) return ERROR_INVALID_PARAMETER;
    
    table->rows[row_index].is_checked = checked;
    
    if (table->on_check) {
        table->on_check();
    }
    
    return ERROR_NONE;
}