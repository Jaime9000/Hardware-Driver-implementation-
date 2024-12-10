#include "windows_api.h"
#include "serialize_deserialize.h"
#include "logger.h"

static HANDLE watch_thread = NULL;
static RedrawCallback global_callback = NULL;
static volatile bool should_watch = false;

ErrorCode check_c_serialize_bucket(void) {
    if (!CreateDirectory(COORDINATES_FILE_DIR_PATH, NULL) && 
        GetLastError() != ERROR_ALREADY_EXISTS) {
        log_error("Failed to create c_wrapper_serialize_bucket directory");
        return ERROR_WRITE_FAILED;
    }
    return ERROR_NONE;
}

static void create_default_coordinates(CoordinatesData* coordinates) {
    RECT desktop_rect;
    GetWindowRect(GetDesktopWindow(), &desktop_rect);
    
    coordinates->left.x = 0;
    coordinates->left.y = (desktop_rect.bottom - desktop_rect.top) / 8;
    coordinates->left.size = 120;
    
    coordinates->right.x = desktop_rect.right - 500;
    coordinates->right.y = (desktop_rect.bottom - desktop_rect.top) / 8;
    coordinates->right.size = 120;
}

ErrorCode load_coordinates(CoordinatesData* coordinates) {
    if (!coordinates) return ERROR_INVALID_PARAMETER;
    
    ErrorCode result = check_c_serialize_bucket();
    if (result != ERROR_NONE) return result;

    // Try to deserialize from file
    result = deserialize_from_file(COORDINATES_FILE_PATH, coordinates, sizeof(CoordinatesData));
    if (result != ERROR_NONE) {
        // If file doesn't exist or can't be read, create default coordinates
        create_default_coordinates(coordinates);
        return save_coordinates(coordinates);
    }

    return ERROR_NONE;
}

ErrorCode save_coordinates(const CoordinatesData* coordinates) {
    if (!coordinates) return ERROR_INVALID_PARAMETER;

    ErrorCode result = check_c_serialize_bucket();
    if (result != ERROR_NONE) return result;

    // Serialize to file
    return serialize_to_file(COORDINATES_FILE_PATH, coordinates, sizeof(CoordinatesData));
}

ErrorCode load_placement_values(bool is_left, int* x, int* y, int* size) {
    if (!x || !y || !size) return ERROR_INVALID_PARAMETER;

    CoordinatesData coordinates;
    ErrorCode result = load_coordinates(&coordinates);
    if (result != ERROR_NONE) return result;

    WindowPlacement* placement = is_left ? &coordinates.left : &coordinates.right;
    *x = placement->x;
    *y = placement->y;
    *size = placement->size;

    return ERROR_NONE;
}

static BOOL CALLBACK enum_windows_callback(HWND hwnd, LPARAM _) {
    if (!IsWindowVisible(hwnd)) return TRUE;

    char window_title[MAX_WINDOW_TITLE];
    GetWindowText(hwnd, window_title, MAX_WINDOW_TITLE);
    
    if (strstr(window_title, "K7 Version 17")) {
        SetActiveWindow(hwnd);
        return FALSE;
    }
    return TRUE;
}

ErrorCode make_k7_window_active(void) {
    EnumWindows(enum_windows_callback, 0);
    return ERROR_NONE;
}

static DWORD WINAPI watch_directory(LPVOID _) {
    while (should_watch) {
        HANDLE change = FindFirstChangeNotification(
            COORDINATES_FILE_DIR_PATH,
            TRUE,
            FILE_NOTIFY_CHANGE_LAST_WRITE | 
            FILE_NOTIFY_CHANGE_FILE_NAME |
            FILE_NOTIFY_CHANGE_DIR_NAME
        );

        if (change == INVALID_HANDLE_VALUE) {
            Sleep(1000); // Wait before retry
            continue;
        }

        DWORD wait_result;
        while (should_watch && 
               (wait_result = WaitForSingleObject(change, 100)) != WAIT_FAILED) {
            if (wait_result == WAIT_OBJECT_0) {
                if (global_callback) global_callback();
                if (!FindNextChangeNotification(change)) break;
            }
        }

        FindCloseChangeNotification(change);
        
        if (should_watch) {
            Sleep(1000); // Wait before restarting watch
        }
    }
    return 0;
}

ErrorCode setup_watch_event(RedrawCallback callback) {
    if (!callback) return ERROR_INVALID_PARAMETER;
    
    global_callback = callback;
    should_watch = true;

    watch_thread = CreateThread(
        NULL, 0, watch_directory, NULL, 0, NULL
    );

    if (!watch_thread) {
        log_error("Failed to create watch thread");
        return ERROR_THREAD_CREATE;
    }

    return ERROR_NONE;
}

ErrorCode stop_watch_event(void) {
    if (!watch_thread) return ERROR_NONE;

    should_watch = false;
    WaitForSingleObject(watch_thread, INFINITE);
    CloseHandle(watch_thread);
    watch_thread = NULL;
    global_callback = NULL;

    return ERROR_NONE;
}

ErrorCode windows_api_cleanup(void) {
    ErrorCode result = stop_watch_event();
    // Additional cleanup as needed
    return result;
}

ErrorCode restart_file_watcher(void) {
    ErrorCode result = stop_watch_event();
    if (result != ERROR_NONE) return result;
    
    return setup_watch_event(global_callback);
}