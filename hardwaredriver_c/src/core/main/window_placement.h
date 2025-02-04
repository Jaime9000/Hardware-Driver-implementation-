#ifndef MYOTRONICS_WINDOW_PLACEMENT_H
#define MYOTRONICS_WINDOW_PLACEMENT_H

#include <windows.h>
#include "src/core/commands.h"
#include "src/core/error_codes.h"

#define MAX_WINDOW_ID_LENGTH 64
#define PLACEMENT_FILE_PATH "C:\\K7\\window_placement.dat"

typedef struct {
    int x;
    int y;
    int width;
    int height;
    BOOL is_maximized;
    char window_id[MAX_WINDOW_ID_LENGTH];
} WindowPlacement;

// Initialization and cleanup
ErrorCode window_placement_init(void);
void window_placement_cleanup(void);

// Window placement operations
ErrorCode window_placement_set(HWND hwnd, const WindowPlacement* placement);
ErrorCode window_placement_get(HWND hwnd, WindowPlacement* placement);

// Storage operations
ErrorCode window_placement_save(const char* window_id, const WindowPlacement* placement);
ErrorCode window_placement_load(const char* window_id, WindowPlacement* placement);

// Utility functions
ErrorCode window_placement_center_window(HWND hwnd);
ErrorCode window_placement_get_monitor_info(HMONITOR hMonitor, MONITORINFO* monitor_info);

#endif // MYOTRONICS_WINDOW_PLACEMENT_H