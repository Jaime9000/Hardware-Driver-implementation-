#ifndef MYOTRONICS_CONFIG_H
#define MYOTRONICS_CONFIG_H

#include <stdbool.h>
#include <windows.h>
#include "../logger.h"
#include "../error_codes.h"
#include "cJSON.h"

// File paths and constants
#define MAX_PORT_NAME_LENGTH 256
#define MAX_PATH_LENGTH 256
#define DEFAULT_CONFIG_PATH "C:\\K7\\config.json"
#define DEFAULT_LOG_DIR "C:\\K7\\logs"
#define DEFAULT_FREQ_CONFIG_PATH "C:\\K7\\freq_config"
#define USB_VENDOR_ID_2303 "2303"
#define USB_VENDOR_ID_23A3 "23A3"

// Error codes for config operations
typedef enum {
    CONFIG_SUCCESS = 0,
    CONFIG_ERROR_NO_PORTS = -1,
    CONFIG_ERROR_PORT_ACCESS = -2,
    CONFIG_ERROR_LOG_ACCESS = -3,
    CONFIG_ERROR_MEMORY = -4,
    CONFIG_ERROR_FILE_ACCESS = -5,
    CONFIG_ERROR_INVALID_CONFIG = -6,
    CONFIG_ERROR_JSON_PARSE = -7
} ConfigError;

// Configuration structure matching Python implementation
typedef struct {
    // Paths
    char config_path[MAX_PATH_LENGTH];
    char log_dir[MAX_PATH_LENGTH];
    char freq_config_path[MAX_PATH_LENGTH];
    
    // Serial port configuration
    char port_name[MAX_PORT_NAME_LENGTH];
    bool port_auto_detect;
    
    // Operation mode
    bool is_service;
    bool debug_enabled;
    bool info_enabled;
    bool debug_events;
    
    // Sample configuration
    int sample_count;
    
    // Internal state
    bool is_initialized;
} Config;

// Constructor/Destructor
Config* config_create(void);
void config_destroy(Config* config);

// Initialization and configuration
ConfigError config_init(Config* config);
ConfigError config_load_defaults(Config* config);
ConfigError config_load_from_file(Config* config, const char* config_path);
ConfigError config_save_to_file(Config* config, const char* config_path);
ConfigError config_validate(Config* config);

// Port management
ConfigError config_sense_ports(Config* config);
const char* config_get_port_name(const Config* config);
ConfigError config_set_port_name(Config* config, const char* port_name);

// Frequency configuration
ConfigError config_get_frequency(Config* config, int* frequency);
ConfigError config_set_frequency(Config* config, int frequency);

// Service mode
void config_set_service_mode(Config* config, bool enabled);
bool config_is_service_mode(const Config* config);

// Debug settings
void config_set_debug(Config* config, bool enabled);
void config_set_info(Config* config, bool enabled);
void config_set_debug_events(Config* config, bool enabled);

// Sample configuration
void config_set_sample_count(Config* config, int count);
int config_get_sample_count(const Config* config);

// Path management
ConfigError config_set_log_dir(Config* config, const char* log_dir);
const char* config_get_log_dir(const Config* config);

// Utility functions
const char* config_get_error_string(ConfigError error);
bool config_is_initialized(const Config* config);
ConfigError config_ensure_directories(Config* config);

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif // MYOTRONICS_CONFIG_H