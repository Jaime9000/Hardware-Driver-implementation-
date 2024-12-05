#ifndef WINDOWS_API_H
#define WINDOWS_API_H

#include <windows.h>
#include <stdbool.h>
#include "error_codes.h"

#define COORDINATES_FILE_DIR_PATH "C:\\K7\\python"
#define COORDINATES_FILE_PATH "C:\\K7\\python\\sweep_window_coordinates.pkl"
#define MAX_WINDOW_TITLE 256

typedef struct {
    int x;
    int y;
    int size;
} WindowPlacement;

typedef struct {
    WindowPlacement left;
    WindowPlacement right;
} CoordinatesData;

typedef void (*RedrawCallback)(void);

// File operations
ErrorCode check_python_bucket(void);
ErrorCode load_coordinates(CoordinatesData* coordinates);
ErrorCode save_coordinates(const CoordinatesData* coordinates);

// Window placement
ErrorCode load_placement_values(bool is_left, int* x, int* y, int* size);
ErrorCode make_k7_window_active(void);

// File watching
ErrorCode setup_watch_event(RedrawCallback callback);
ErrorCode stop_watch_event(void);

ErrorCode windows_api_cleanup(void);
ErrorCode restart_file_watcher(void);

#endif // WINDOWS_API_H