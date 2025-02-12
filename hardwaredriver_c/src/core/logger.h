#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <windows.h>
#include <stdint.h>
#include "src/core/error_codes.h"

// Log levels matching Python's logging levels
typedef enum {
    LOG_LEVEL_CRITICAL = 0,
    LOG_LEVEL_ERROR = 1,
    LOG_LEVEL_WARNING = 2,
    LOG_LEVEL_INFO = 3,
    LOG_LEVEL_DEBUG = 4
} LogLevel;

// Global logger instance
typedef struct Logger {
    FILE* log_file;
    char log_path[MAX_PATH];
    LogLevel level;
    char module_name[64];  // For context tracking like Python's __name__
} Logger;

// Core logger functions
ErrorCode logger_init(const char* log_dir);
void logger_cleanup(void);
void logger_set_level(LogLevel level);
void logger_set_module(const char* module_name);

// Logging functions (matching Python's logging interface)
void log_critical(const char* format, ...);
void log_error(const char* format, ...);
void log_warning(const char* format, ...);
void log_info(const char* format, ...);
void log_debug(const char* format, ...);

// Enhanced debugging support
void log_debug_buffer(const char* prefix, const uint8_t* data, size_t length);
void log_debug_error(ErrorCode code, const char* file, int line, const char* func);

#endif // LOGGER_H