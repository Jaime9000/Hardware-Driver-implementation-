#ifndef WINDOWS_API_H
#define WINDOWS_API_H

// System headers
#include <windows.h>
#include <stdbool.h>

// Project headers
#include "src/core/error_codes.h"

// Constants
#define COORDINATES_FILE_DIR_PATH "C:\\K7\\c_wrapper_serialize_bucket"
#define COORDINATES_FILE_PATH "C:\\K7\\c_wrapper_serialize_bucket\\sweep_window_coordinates.dat"
#define MAX_WINDOW_TITLE 256

/**
 * @brief Structure to store window position and size
 */
typedef struct {
    int x;          // Window x-coordinate
    int y;          // Window y-coordinate
    int size;       // Window size
} WindowPlacement;

/**
 * @brief Structure to store coordinates for left and right windows
 */
typedef struct {
    WindowPlacement left;     // Left window placement
    WindowPlacement right;    // Right window placement
} CoordinatesData;

/**
 * @brief Callback function type for window redraw events
 */
typedef void (*RedrawCallback)(void);

// File operations
/**
 * @brief Ensures the coordinates directory exists
 * @return ErrorCode ERROR_NONE on success, error code otherwise
 */
ErrorCode check_c_serialize_bucket(void);

/**
 * @brief Loads window coordinates from file
 * @param[out] coordinates Pointer to store loaded coordinates
 * @return ErrorCode ERROR_NONE on success, error code otherwise
 */
ErrorCode load_coordinates(CoordinatesData* coordinates);

/**
 * @brief Saves window coordinates to file
 * @param coordinates Pointer to coordinates to save
 * @return ErrorCode ERROR_NONE on success, error code otherwise
 */
ErrorCode save_coordinates(const CoordinatesData* coordinates);

// Window placement
/**
 * @brief Loads placement values for a specific window
 * @param is_left True for left window, false for right
 * @param[out] x Pointer to store x-coordinate
 * @param[out] y Pointer to store y-coordinate
 * @param[out] size Pointer to store window size
 * @return ErrorCode ERROR_NONE on success, error code otherwise
 */
ErrorCode load_placement_values(bool is_left, int* x, int* y, int* size);

/**
 * @brief Makes the K7 window active
 * @return ErrorCode ERROR_NONE on success, error code otherwise
 */
ErrorCode make_k7_window_active(void);

// File watching
/**
 * @brief Sets up file system watcher
 * @param callback Function to call when changes detected
 * @return ErrorCode ERROR_NONE on success, error code otherwise
 */
ErrorCode setup_watch_event(RedrawCallback callback);

/**
 * @brief Stops the file system watcher
 * @return ErrorCode ERROR_NONE on success, error code otherwise
 */
ErrorCode stop_watch_event(void);

/**
 * @brief Cleans up Windows API resources
 * @return ErrorCode ERROR_NONE on success, error code otherwise
 */
ErrorCode windows_api_cleanup(void);

/**
 * @brief Restarts the file system watcher
 * @return ErrorCode ERROR_NONE on success, error code otherwise
 */
ErrorCode restart_file_watcher(void);

#endif // WINDOWS_API_H