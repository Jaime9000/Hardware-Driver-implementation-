#include "window_placement.h"
#include <stdio.h>
#include <string.h>

// File format version for compatibility
#define PLACEMENT_FILE_VERSION 1

// Structure for file header
typedef struct {
    DWORD version;
    DWORD entry_count;
} PlacementFileHeader;

// Static variables
static CRITICAL_SECTION placement_mutex;
static BOOL is_initialized = FALSE;

ErrorCode window_placement_init(void) {
    if (is_initialized) {
        return ERROR_NONE;
    }

    InitializeCriticalSection(&placement_mutex);
    is_initialized = TRUE;

    // Ensure the K7 directory exists
    if (!CreateDirectory("C:\\K7", NULL) && 
        GetLastError() != ERROR_ALREADY_EXISTS) {
        return ERROR_WRITE_FAILED;
    }

    return ERROR_NONE;
}

void window_placement_cleanup(void) {
    if (is_initialized) {
        DeleteCriticalSection(&placement_mutex);
        is_initialized = FALSE;
    }
}

ErrorCode window_placement_set(HWND hwnd, const WindowPlacement* placement) {
    if (!hwnd || !placement) {
        return ERROR_INVALID_COMMAND;
    }

    WINDOWPLACEMENT wp;
    wp.length = sizeof(WINDOWPLACEMENT);
    wp.flags = 0;
    wp.showCmd = placement->is_maximized ? SW_MAXIMIZE : SW_NORMAL;
    wp.ptMinPosition.x = 0;
    wp.ptMinPosition.y = 0;
    wp.ptMaxPosition.x = 0;
    wp.ptMaxPosition.y = 0;
    wp.rcNormalPosition.left = placement->x;
    wp.rcNormalPosition.top = placement->y;
    wp.rcNormalPosition.right = placement->x + placement->width;
    wp.rcNormalPosition.bottom = placement->y + placement->height;

    if (!SetWindowPlacement(hwnd, &wp)) {
        return ERROR_WRITE_FAILED;
    }

    return ERROR_NONE;
}

ErrorCode window_placement_get(HWND hwnd, WindowPlacement* placement) {
    if (!hwnd || !placement) {
        return ERROR_INVALID_COMMAND;
    }

    WINDOWPLACEMENT wp;
    wp.length = sizeof(WINDOWPLACEMENT);

    if (!GetWindowPlacement(hwnd, &wp)) {
        return ERROR_READ_FAILED;
    }

    placement->x = wp.rcNormalPosition.left;
    placement->y = wp.rcNormalPosition.top;
    placement->width = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
    placement->height = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
    placement->is_maximized = (wp.showCmd == SW_MAXIMIZE);

    return ERROR_NONE;
}

ErrorCode window_placement_save(const char* window_id, const WindowPlacement* placement) {
    if (!window_id || !placement) {
        return ERROR_INVALID_COMMAND;
    }

    EnterCriticalSection(&placement_mutex);

    FILE* file = fopen(PLACEMENT_FILE_PATH, "rb+");
    if (!file) {
        // Create new file if it doesn't exist
        file = fopen(PLACEMENT_FILE_PATH, "wb+");
        if (!file) {
            LeaveCriticalSection(&placement_mutex);
            return ERROR_WRITE_FAILED;
        }

        // Write initial header
        PlacementFileHeader header = {PLACEMENT_FILE_VERSION, 0};
        fwrite(&header, sizeof(header), 1, file);
    }

    // Read header
    PlacementFileHeader header;
    fseek(file, 0, SEEK_SET);
    fread(&header, sizeof(header), 1, file);

    // Search for existing entry
    BOOL found = FALSE;
    WindowPlacement existing_placement;
    long position = sizeof(header);

    for (DWORD i = 0; i < header.entry_count; i++) {
        fseek(file, position, SEEK_SET);
        fread(&existing_placement, sizeof(WindowPlacement), 1, file);
        
        if (strcmp(existing_placement.window_id, window_id) == 0) {
            found = TRUE;
            break;
        }
        position += sizeof(WindowPlacement);
    }

    if (found) {
        // Update existing entry
        fseek(file, position, SEEK_SET);
    } else {
        // Add new entry
        fseek(file, 0, SEEK_END);
        header.entry_count++;
        fseek(file, 0, SEEK_SET);
        fwrite(&header, sizeof(header), 1, file);
    }

    // Write placement data
    WindowPlacement save_placement = *placement;
    strncpy(save_placement.window_id, window_id, MAX_WINDOW_ID_LENGTH - 1);
    save_placement.window_id[MAX_WINDOW_ID_LENGTH - 1] = '\0';
    
    fwrite(&save_placement, sizeof(WindowPlacement), 1, file);
    fclose(file);

    LeaveCriticalSection(&placement_mutex);
    return ERROR_NONE;
}

ErrorCode window_placement_load(const char* window_id, WindowPlacement* placement) {
    if (!window_id || !placement) {
        return ERROR_INVALID_COMMAND;
    }

    EnterCriticalSection(&placement_mutex);

    FILE* file = fopen(PLACEMENT_FILE_PATH, "rb");
    if (!file) {
        LeaveCriticalSection(&placement_mutex);
        return ERROR_READ_FAILED;
    }

    // Read header
    PlacementFileHeader header;
    fread(&header, sizeof(header), 1, file);

    // Search for window_id
    WindowPlacement temp_placement;
    BOOL found = FALSE;

    for (DWORD i = 0; i < header.entry_count; i++) {
        fread(&temp_placement, sizeof(WindowPlacement), 1, file);
        if (strcmp(temp_placement.window_id, window_id) == 0) {
            *placement = temp_placement;
            found = TRUE;
            break;
        }
    }

    fclose(file);
    LeaveCriticalSection(&placement_mutex);

    return found ? ERROR_NONE : ERROR_READ_FAILED;
}

ErrorCode window_placement_center_window(HWND hwnd) {
    if (!hwnd) {
        return ERROR_INVALID_COMMAND;
    }

    HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO monitor_info;
    monitor_info.cbSize = sizeof(MONITORINFO);

    if (!GetMonitorInfo(hMonitor, &monitor_info)) {
        return ERROR_READ_FAILED;
    }

    RECT window_rect;
    if (!GetWindowRect(hwnd, &window_rect)) {
        return ERROR_READ_FAILED;
    }

    int window_width = window_rect.right - window_rect.left;
    int window_height = window_rect.bottom - window_rect.top;
    int monitor_width = monitor_info.rcWork.right - monitor_info.rcWork.left;
    int monitor_height = monitor_info.rcWork.bottom - monitor_info.rcWork.top;

    int x = monitor_info.rcWork.left + (monitor_width - window_width) / 2;
    int y = monitor_info.rcWork.top + (monitor_height - window_height) / 2;

    if (!SetWindowPos(hwnd, NULL, x, y, 0, 0, 
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE)) {
        return ERROR_WRITE_FAILED;
    }

    return ERROR_NONE;
}

ErrorCode window_placement_get_monitor_info(HMONITOR hMonitor, MONITORINFO* monitor_info) {
    if (!hMonitor || !monitor_info) {
        return ERROR_INVALID_COMMAND;
    }

    monitor_info->cbSize = sizeof(MONITORINFO);
    if (!GetMonitorInfo(hMonitor, monitor_info)) {
        return ERROR_READ_FAILED;
    }

    return ERROR_NONE;
}