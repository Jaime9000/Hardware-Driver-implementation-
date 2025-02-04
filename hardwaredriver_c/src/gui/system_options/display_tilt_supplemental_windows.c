#include "src/gui/system_options/display_tilt_supplemental_windows.h"
#include "src/core/logger.h"

// System headers
#include <stdio.h>
#include <sys/stat.h>
#include <windows.h>

// Constants
#define CONFIG_VALUE_SHOW '1'
#define CONFIG_VALUE_HIDE '0'

/**
 * @brief Ensures the K7 directory exists
 * 
 * @return ErrorCode ERROR_NONE on success, error code otherwise
 */
static ErrorCode ensure_directory_exists(void) {
    #ifdef _WIN32
    if (CreateDirectoryA("C:\\K7", NULL) == 0) {
        DWORD error = GetLastError();
        if (error != ERROR_ALREADY_EXISTS) {
            log_error("Failed to create directory C:\\K7: error %lu", error);
            return ERROR_FILE_OPERATION;
        }
    }
    #endif
    return ERROR_NONE;
}

/**
 * @brief Writes a configuration value to the settings file
 * 
 * @param value Single character value to write ('0' or '1')
 * @return ErrorCode ERROR_NONE on success, error code otherwise
 */
static ErrorCode write_config(const char* value) {
    if (!value) {
        log_error("Invalid value parameter");
        return ERROR_INVALID_PARAMETER;
    }

    ErrorCode err = ensure_directory_exists();
    if (err != ERROR_NONE) {
        log_error("Failed to ensure directory exists");
        return err;
    }

    FILE* fp = fopen(SUPPLEMENTAL_WINDOW_DISPLAY_SETTING_PATH, "w");
    if (!fp) {
        log_error("Failed to open config file for writing");
        return ERROR_FILE_OPERATION;
    }
    
    size_t written = fwrite(value, 1, 1, fp);
    fclose(fp);
    
    return written == 1 ? ERROR_NONE : ERROR_FILE_OPERATION;
}


/**
 * @brief Checks if a file exists
 * 
 * @param path Path to file
 * @return bool true if file exists, false otherwise
 */
static bool file_exists(const char* path) {
    if (!path) return false;
    
    struct stat buffer;
    return stat(path, &buffer) == 0;
}

ErrorCode read_config_tilt_supplemental_windows(bool* show_windows) {
    if (!show_windows) {
        log_error("Invalid show_windows parameter");
        return ERROR_INVALID_PARAMETER;
    }

    // Create file if it doesn't exist
    if (!file_exists(SUPPLEMENTAL_WINDOW_DISPLAY_SETTING_PATH)) {
        log_info("Config file doesn't exist, creating with default value");
        ErrorCode err = show_config_tilt_supplemental_windows();
        if (err != ERROR_NONE) {
            log_error("Failed to create default config");
            return err;
        }
    }

    // Read configuration
    FILE* fp = fopen(SUPPLEMENTAL_WINDOW_DISPLAY_SETTING_PATH, "r");
    if (!fp) {
        log_error("Failed to open config file for reading");
        return ERROR_FILE_OPERATION;
    }

    char value;
    size_t read = fread(&value, 1, 1, fp);
    fclose(fp);

    if (read != 1) {
        // Handle empty file case
        log_warning("Empty config file, setting default value");
        ErrorCode err = show_config_tilt_supplemental_windows();
        if (err != ERROR_NONE) return err;
        *show_windows = true;
    } else {
        //*show_windows = (value == '1');
        *show_windows = (value == CONFIG_VALUE_SHOW);
    }

    return ERROR_NONE;
}

ErrorCode show_config_tilt_supplemental_windows(void) {
    //return write_config("1");
    return write_config(&CONFIG_VALUE_SHOW);
}

ErrorCode hide_config_tilt_supplemental_windows(void) {
    //return write_config("0");
    return write_config(&CONFIG_VALUE_HIDE);
}