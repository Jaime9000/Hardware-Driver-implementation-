#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <direct.h>  // For _mkdir
#include <setupapi.h>
#include <initguid.h>
#include <devguid.h>

// Static helper function declarations
static void get_timestamp(char* buffer, size_t size);
static ConfigError rotate_log_if_needed(Config* config);
static ConfigError ensure_directory_exists(const char* path);
static bool is_target_usb_device(const char* hardware_id);

// Constructor
Config* config_create(void) {
    Config* config = (Config*)malloc(sizeof(Config));
    if (config == NULL) {
        return NULL;
    }

    memset(config, 0, sizeof(Config));
    config_load_defaults(config);
    return config;
}

// Destructor
void config_destroy(Config* config) {
    if (config) {
        free(config);
    }
}

// Initialize with defaults
ConfigError config_load_defaults(Config* config) {
    if (!config) {
        return CONFIG_ERROR_MEMORY;
    }

    // Set default values
    config->port_auto_detect = true;
    config->log_level = LOG_LEVEL_INFO;
    config->rotating_log = true;
    config->max_log_size = 10 * 1024 * 1024;  // 10MB
    config->is_service = false;
    config->debug_enabled = false;
    config->info_enabled = true;
    config->debug_events = false;
    config->sample_count = 1600;
    config->is_initialized = false;

    // Set default paths
    strncpy(config->log_file_path, DEFAULT_LOG_PATH, MAX_LOG_PATH_LENGTH - 1);
    config->log_file_path[MAX_LOG_PATH_LENGTH - 1] = '\0';

    return CONFIG_SUCCESS;
}

// Port detection implementation
ConfigError config_sense_ports(Config* config) {
    if (!config) {
        return CONFIG_ERROR_MEMORY;
    }

    HDEVINFO device_info_set = SetupDiGetClassDevs(
        &GUID_DEVCLASS_PORTS, NULL, NULL, 
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

    if (device_info_set == INVALID_HANDLE_VALUE) {
        return CONFIG_ERROR_PORT_ACCESS;
    }

    SP_DEVINFO_DATA device_info_data;
    device_info_data.cbSize = sizeof(SP_DEVINFO_DATA);
    
    DWORD device_index = 0;
    bool found_port = false;

    while (SetupDiEnumDeviceInfo(device_info_set, device_index, &device_info_data)) {
        char hardware_id[256];
        if (SetupDiGetDeviceRegistryProperty(
                device_info_set, &device_info_data,
                SPDRP_HARDWAREID, NULL,
                (PBYTE)hardware_id, sizeof(hardware_id), NULL)) {
            
            if (is_target_usb_device(hardware_id)) {
                char port_name[MAX_PORT_NAME_LENGTH];
                HKEY key = SetupDiOpenDevRegKey(
                    device_info_set, &device_info_data,
                    DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
                
                if (key != INVALID_HANDLE_VALUE) {
                    DWORD type;
                    DWORD size = sizeof(port_name);
                    if (RegQueryValueEx(key, "PortName", NULL, &type, 
                                      (LPBYTE)port_name, &size) == ERROR_SUCCESS) {
                        strncpy(config->port_name, port_name, MAX_PORT_NAME_LENGTH - 1);
                        found_port = true;
                        RegCloseKey(key);
                        break;
                    }
                    RegCloseKey(key);
                }
            }
        }
        device_index++;
    }

    SetupDiDestroyDeviceInfoList(device_info_set);
    return found_port ? CONFIG_SUCCESS : CONFIG_ERROR_NO_PORTS;
}

// Logging implementation
ConfigError config_init_logging(Config* config) {
    if (!config) {
        return CONFIG_ERROR_MEMORY;
    }

    // Ensure the log directory exists
    ConfigError err = ensure_directory_exists(config->log_file_path);
    if (err != CONFIG_SUCCESS) {
        return err;
    }

    // Test if we can write to the log file
    FILE* test_file = fopen(config->log_file_path, "a");
    if (!test_file) {
        return CONFIG_ERROR_LOG_ACCESS;
    }
    fclose(test_file);

    return CONFIG_SUCCESS;
}

void config_log(Config* config, LogLevel level, const char* format, ...) {
    if (!config || level > config->log_level) {
        return;
    }

    // Rotate log if needed
    rotate_log_if_needed(config);

    FILE* log_file = fopen(config->log_file_path, "a");
    if (!log_file) {
        return;
    }

    // Get timestamp
    char timestamp[26];
    get_timestamp(timestamp, sizeof(timestamp));

    // Get log level string
    const char* level_str;
    switch (level) {
        case LOG_LEVEL_ERROR: level_str = "ERROR"; break;
        case LOG_LEVEL_INFO:  level_str = "INFO"; break;
        case LOG_LEVEL_DEBUG: level_str = "DEBUG"; break;
        default: level_str = "UNKNOWN"; break;
    }

    // Write timestamp and level
    fprintf(log_file, "[%s] [%s] ", timestamp, level_str);

    // Write formatted message
    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);

    fprintf(log_file, "\n");
    fclose(log_file);
}

// Frequency configuration
ConfigError config_get_frequency(Config* config, int* frequency) {
    FILE* freq_file = fopen(FREQ_CONFIG_PATH, "r");
    if (!freq_file) {
        *frequency = 60;  // Default to 60Hz
        return CONFIG_SUCCESS;
    }

    char freq_str[4];
    if (fgets(freq_str, sizeof(freq_str), freq_file) != NULL) {
        *frequency = atoi(freq_str);
    } else {
        *frequency = 60;  // Default to 60Hz
    }

    fclose(freq_file);
    return CONFIG_SUCCESS;
}

ConfigError config_set_frequency(Config* config, int frequency) {
    if (frequency != 50 && frequency != 60) {
        return CONFIG_ERROR_MEMORY;  // Invalid frequency
    }

    FILE* freq_file = fopen(FREQ_CONFIG_PATH, "w");
    if (!freq_file) {
        return CONFIG_ERROR_LOG_ACCESS;
    }

    fprintf(freq_file, "%d", frequency);
    fclose(freq_file);
    return CONFIG_SUCCESS;
}

// Helper function implementations
static void get_timestamp(char* buffer, size_t size) {
    time_t now;
    time(&now);
    struct tm* tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

static ConfigError rotate_log_if_needed(Config* config) {
    if (!config->rotating_log) {
        return CONFIG_SUCCESS;
    }

    FILE* log_file = fopen(config->log_file_path, "r");
    if (!log_file) {
        return CONFIG_SUCCESS;  // File doesn't exist yet
    }

    // Get file size
    fseek(log_file, 0, SEEK_END);
    long size = ftell(log_file);
    fclose(log_file);

    if (size >= config->max_log_size) {
        char backup_path[MAX_LOG_PATH_LENGTH];
        snprintf(backup_path, sizeof(backup_path), "%s.old", config->log_file_path);
        
        // Remove old backup if it exists
        remove(backup_path);
        
        // Rename current log to backup
        rename(config->log_file_path, backup_path);
    }

    return CONFIG_SUCCESS;
}

static ConfigError ensure_directory_exists(const char* path) {
    char dir_path[MAX_LOG_PATH_LENGTH];
    strncpy(dir_path, path, sizeof(dir_path));

    // Find last separator
    char* last_sep = strrchr(dir_path, '\\');
    if (last_sep) {
        *last_sep = '\0';
        
        // Create directory if it doesn't exist
        if (_mkdir(dir_path) != 0 && errno != EEXIST) {
            return CONFIG_ERROR_LOG_ACCESS;
        }
    }

    return CONFIG_SUCCESS;
}

static bool is_target_usb_device(const char* hardware_id) {
    return (strstr(hardware_id, USB_VENDOR_ID_2303) != NULL ||
            strstr(hardware_id, USB_VENDOR_ID_23A3) != NULL);
}

// Getter/Setter implementations
const char* config_get_port_name(const Config* config) {
    return config ? config->port_name : NULL;
}

ConfigError config_set_port_name(Config* config, const char* port_name) {
    if (!config || !port_name) {
        return CONFIG_ERROR_MEMORY;
    }
    strncpy(config->port_name, port_name, MAX_PORT_NAME_LENGTH - 1);
    config->port_name[MAX_PORT_NAME_LENGTH - 1] = '\0';
    return CONFIG_SUCCESS;
}

void config_set_service_mode(Config* config, bool enabled) {
    if (config) {
        config->is_service = enabled;
    }
}

bool config_is_service_mode(const Config* config) {
    return config ? config->is_service : false;
}

const char* config_get_error_string(ConfigError error) {
    switch (error) {
        case CONFIG_SUCCESS: return "Success";
        case CONFIG_ERROR_NO_PORTS: return "No compatible ports found";
        case CONFIG_ERROR_PORT_ACCESS: return "Cannot access port";
        case CONFIG_ERROR_LOG_ACCESS: return "Cannot access log file";
        case CONFIG_ERROR_MEMORY: return "Memory error";
        default: return "Unknown error";
    }
}