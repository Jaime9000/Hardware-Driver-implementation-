#ifndef DISPLAY_TILT_SUPPLEMENTAL_WINDOWS_H
#define DISPLAY_TILT_SUPPLEMENTAL_WINDOWS_H

// System headers
#include <stdio.h>
#include <stdbool.h>

// Project headers
#include "src/core/error_codes.h"

// Path constants
#ifdef _WIN32
#define SUPPLEMENTAL_WINDOW_DISPLAY_SETTING_PATH "C:\\K7\\options_display"
#else
#define SUPPLEMENTAL_WINDOW_DISPLAY_SETTING_PATH "./options_display"
#endif

/**
 * @brief Reads the configuration for tilt supplemental windows display
 * 
 * @param[out] show_windows Pointer to bool to store display state
 * @return ErrorCode ERROR_NONE on success, error code otherwise
 * @note Creates config file with default value (show) if it doesn't exist
 */
ErrorCode read_config_tilt_supplemental_windows(bool* show_windows);

/**
 * @brief Sets configuration to show tilt supplemental windows
 * 
 * @return ErrorCode ERROR_NONE on success, error code otherwise
 */
ErrorCode show_config_tilt_supplemental_windows(void);

/**
 * @brief Sets configuration to hide tilt supplemental windows
 * 
 * @return ErrorCode ERROR_NONE on success, error code otherwise
 */
ErrorCode hide_config_tilt_supplemental_windows(void);

#endif // DISPLAY_TILT_SUPPLEMENTAL_WINDOWS_H