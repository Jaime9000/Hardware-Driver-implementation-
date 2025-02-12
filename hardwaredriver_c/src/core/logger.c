#include "src/core/logger.h"
#include "src/core/error_codes.h"

// System headers
// System headers
#include <stdio.h>    // for file operations
#include <stdlib.h>   // for malloc/free
#include <stdarg.h>   // for va_list
#include <string.h>   // for strncpy
#include <time.h>     // for time functions

static Logger* g_logger = NULL;

static const char* LEVEL_STRINGS[] = {
    "CRITICAL",
    "ERROR",
    "WARNING",
    "INFO",
    "DEBUG"
};

// Internal helper for common logging functionality
static void log_message(LogLevel level, const char* format, va_list args) {
    if (!g_logger || level > g_logger->level) return;
    
    // Get timestamp
    SYSTEMTIME st;
    GetLocalTime(&st);
    
    // Print header with context (timestamp, level, module)
    fprintf(g_logger->log_file, 
            "[%04d-%02d-%02d %02d:%02d:%02d.%03d][%-8s][%s] ",
            st.wYear, st.wMonth, st.wDay,
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
            LEVEL_STRINGS[level],
            g_logger->module_name);
    
    // Print message
    vfprintf(g_logger->log_file, format, args);
    fprintf(g_logger->log_file, "\n");
    fflush(g_logger->log_file);
}

ErrorCode logger_init(const char* log_dir) {
    // Allocate logger structure
    g_logger = malloc(sizeof(Logger));
    if (!g_logger) return ERROR_MEMORY_ALLOCATION;
    
    // Create timestamped log file name
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(g_logger->log_path, MAX_PATH,
             "%s\\driver_%04d%02d%02d_%02d%02d%02d.log", 
             log_dir, 
             st.wYear, st.wMonth, st.wDay,
             st.wHour, st.wMinute, st.wSecond);
             
    // Open log file
    g_logger->log_file = fopen(g_logger->log_path, "a");
    if (!g_logger->log_file) {
        free(g_logger);
        g_logger = NULL;
        return ERROR_FILE_ACCESS;
    }
    
    // Initialize other fields
    g_logger->level = LOG_LEVEL_INFO;  // Default level
    strcpy(g_logger->module_name, "main");  // Default module name
    
    // Log initialization
    log_info("Logger initialized");
    return ERROR_NONE;
}

void logger_cleanup(void) {
    if (g_logger) {
        if (g_logger->log_file) {
            log_info("Logger shutting down");
            fclose(g_logger->log_file);
        }
        free(g_logger);
        g_logger = NULL;
    }
}

void logger_set_level(LogLevel level) {
    if (g_logger && level >= LOG_LEVEL_CRITICAL && level <= LOG_LEVEL_DEBUG) {
        g_logger->level = level;
    }
}

void logger_set_module(const char* module_name) {
    if (g_logger && module_name) {
        strncpy(g_logger->module_name, module_name, sizeof(g_logger->module_name) - 1);
        g_logger->module_name[sizeof(g_logger->module_name) - 1] = '\0';
    }
}

// Logging function implementations
void log_critical(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_message(LOG_LEVEL_CRITICAL, format, args);
    va_end(args);
}

void log_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_message(LOG_LEVEL_ERROR, format, args);
    va_end(args);
}

void log_warning(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_message(LOG_LEVEL_WARNING, format, args);
    va_end(args);
}

void log_info(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_message(LOG_LEVEL_INFO, format, args);
    va_end(args);
}

void log_debug(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_message(LOG_LEVEL_DEBUG, format, args);
    va_end(args);
}

// Enhanced debugging support
void log_debug_buffer(const char* prefix, const uint8_t* data, size_t length) {
    if (!g_logger || g_logger->level < LOG_LEVEL_DEBUG) return;
    
    fprintf(g_logger->log_file, "[DEBUG][%s] %s: ", g_logger->module_name, prefix);
    
    for (size_t i = 0; i < length; i++) {
        fprintf(g_logger->log_file, "%02X ", data[i]);
        if ((i + 1) % 16 == 0 && i + 1 < length) {
            fprintf(g_logger->log_file, "\n                     ");
        }
    }
    
    fprintf(g_logger->log_file, "\n");
    fflush(g_logger->log_file);
}

void log_debug_error(ErrorCode code, const char* file, int line, const char* func) {
    if (!g_logger || g_logger->level < LOG_LEVEL_DEBUG) return;
    
    fprintf(g_logger->log_file, 
            "[DEBUG][%s] Error %d in %s:%d (%s)\n",
            g_logger->module_name, code, file, line, func);
    fflush(g_logger->log_file);
}