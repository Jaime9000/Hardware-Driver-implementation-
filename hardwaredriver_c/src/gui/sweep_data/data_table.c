#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "data_table.h"
#include "sweep_utils.h"

#define BUFFER_SIZE 4096
#define MAX_ROWS 1000
#define MAX_COLUMNS 6

struct DataTable {
    Tcl_Interp* interp;
    char* patient_path_data;
    PlaybackCallback playback_callback;
    CheckCallback on_check;
    int column;
    int row;
    HANDLE dir_handle;
    HANDLE thread_handle;
    OVERLAPPED overlapped;
    char buffer[BUFFER_SIZE];
    volatile BOOL should_run;
    Tcl_Obj* checked_values[MAX_ROWS];
    size_t checked_count;
    char* frame_path;
};

static void create_table_widgets(DataTable* table) {
    Tcl_Eval(table->interp, "destroy .table_frame");
    Tcl_Eval(table->interp, "frame .table_frame");
    
    char cmd[256];
    snprintf(cmd, sizeof(cmd), 
             "grid .table_frame -column %d -row %d", 
             table->column, table->row);
    Tcl_Eval(table->interp, cmd);
    
    table->frame_path = strdup(".table_frame");
}

static void bind_table_events(DataTable* table) {
    // Bind scrollable class events
    Tcl_CreateCommand(table->interp, "ScrollHandler", ScrollCallback, 
                     (ClientData)table, NULL);
    Tcl_Eval(table->interp, 
             "bind scrollable <MouseWheel> {ScrollHandler %D}");

    // Bind clickable class events
    Tcl_CreateCommand(table->interp, "ClickHandler", ClickCallback, 
                     (ClientData)table, NULL);
    Tcl_Eval(table->interp, 
             "bind clickable <Button-1> {ClickHandler %W}");
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
            Tcl_Eval(table->interp, 
                     "after idle {RepopulateTable}");
        }
    }
    return 0;
}

static void populate_table_data(DataTable* table) {
    DIR* dir = opendir(table->patient_path_data);
    if (!dir) return;

    struct dirent* entry;
    char headers[MAX_COLUMNS][32] = {"#", "Date Time", "A Flex", "P Ext", "R Flex", "L Flex"};
    int row = 0;

    // Create headers
    for (int i = 0; i < MAX_COLUMNS; i++) {
        char cmd[512];
        const char* color = "black";
        if (strstr("A Flex P Ext", headers[i])) color = "blue";
        if (strstr("R Flex L Flex", headers[i])) color = "red";

        snprintf(cmd, sizeof(cmd),
                "entry %s.h%d -width 10 -fg %s -font {Arial 9 bold};"
                "%s.h%d insert end {%s};"
                "grid %s.h%d -row 0 -column %d",
                table->frame_path, i, color,
                table->frame_path, i, headers[i],
                table->frame_path, i, i);
        Tcl_Eval(table->interp, cmd);
    }

    // Populate data rows
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_REG) continue;
        
        char filepath[PATH_MAX];
        snprintf(filepath, sizeof(filepath), "%s/%s", 
                table->patient_path_data, entry->d_name);
        
        FILE* file = fopen(filepath, "rb");
        if (!file) continue;

        // Read and parse pickle data
        // Implementation depends on pickle format
        // For now, using placeholder data
        for (int col = 0; col < MAX_COLUMNS; col++) {
            char cmd[512];
            snprintf(cmd, sizeof(cmd),
                    "entry %s.c%d%d -width 10 -fg black -font {Arial 9};"
                    "%s.c%d%d insert end {Data %d-%d};"
                    "grid %s.c%d%d -row %d -column %d;"
                    "bindtags %s.c%d%d {scrollable clickable}",
                    table->frame_path, row, col,
                    table->frame_path, row, col, row, col,
                    table->frame_path, row, col, row + 1, col,
                    table->frame_path, row, col);
            Tcl_Eval(table->interp, cmd);
        }

        fclose(file);
        row++;
    }

    closedir(dir);
}

ErrorCode data_table_create(DataTable** table, 
                          Tcl_Interp* interp,
                          const char* patient_path_data,
                          PlaybackCallback playback_callback,
                          int column,
                          int row,
                          CheckCallback on_check) {
    *table = calloc(1, sizeof(DataTable));
    if (!*table) return ERROR_MEMORY_ALLOCATION;

    DataTable* t = *table;
    t->interp = interp;
    t->patient_path_data = strdup(patient_path_data);
    t->playback_callback = playback_callback;
    t->column = column;
    t->row = row;
    t->on_check = on_check;
    t->should_run = TRUE;

    // Setup directory watching
    wchar_t wide_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, patient_path_data, -1, wide_path, MAX_PATH);

    t->dir_handle = CreateFileW(
        wide_path,
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        NULL);

    if (t->dir_handle == INVALID_HANDLE_VALUE) {
        data_table_destroy(t);
        return ERROR_FILE_OPEN;
    }

    t->overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    t->thread_handle = CreateThread(NULL, 0, watch_directory, t, 0, NULL);

    create_table_widgets(t);
    bind_table_events(t);
    populate_table_data(t);

    return ERROR_NONE;
}

void data_table_destroy(DataTable* table) {
    if (table) {
        table->should_run = FALSE;
        if (table->thread_handle) {
            CancelIoEx(table->dir_handle, &table->overlapped);
            WaitForSingleObject(table->thread_handle, INFINITE);
            CloseHandle(table->thread_handle);
            CloseHandle(table->overlapped.hEvent);
            CloseHandle(table->dir_handle);
        }
        free(table->patient_path_data);
        free(table->frame_path);
        free(table);
    }
}