#include "display_tilt_supplemental_windows.h"
#include <stdio.h>
#include <sys/stat.h>

static ErrorCode ensure_directory_exists(void) {
    #ifdef _WIN32
    if (CreateDirectoryA("C:\\K7", NULL) == 0) {
        DWORD error = GetLastError();
        if (error != ERROR_ALREADY_EXISTS) {
            return ERROR_FILE_OPERATION;
        }
    }
    #endif
    return ERROR_NONE;
}

static ErrorCode write_config(const char* value) {
    ErrorCode err = ensure_directory_exists();
    if (err != ERROR_NONE) return err;

    FILE* fp = fopen(SUPPLEMENTAL_WINDOW_DISPLAY_SETTING_PATH, "w");
    if (!fp) return ERROR_FILE_OPERATION;
    
    size_t written = fwrite(value, 1, 1, fp);
    fclose(fp);
    
    return written == 1 ? ERROR_NONE : ERROR_FILE_OPERATION;
}

static bool file_exists(const char* path) {
    struct stat buffer;
    return stat(path, &buffer) == 0;
}

ErrorCode read_config_tilt_supplemental_windows(bool* show_windows) {
    if (!show_windows) return ERROR_INVALID_PARAMETER;

    // Create file if it doesn't exist
    if (!file_exists(SUPPLEMENTAL_WINDOW_DISPLAY_SETTING_PATH)) {
        ErrorCode err = show_config_tilt_supplemental_windows();
        if (err != ERROR_NONE) return err;
    }

    // Read configuration
    FILE* fp = fopen(SUPPLEMENTAL_WINDOW_DISPLAY_SETTING_PATH, "r");
    if (!fp) return ERROR_FILE_OPERATION;

    char value;
    size_t read = fread(&value, 1, 1, fp);
    fclose(fp);

    if (read != 1) {
        // Handle empty file case
        ErrorCode err = show_config_tilt_supplemental_windows();
        if (err != ERROR_NONE) return err;
        *show_windows = true;
    } else {
        *show_windows = (value == '1');
    }

    return ERROR_NONE;
}

ErrorCode show_config_tilt_supplemental_windows(void) {
    return write_config("1");
}

ErrorCode hide_config_tilt_supplemental_windows(void) {
    return write_config("0");
}