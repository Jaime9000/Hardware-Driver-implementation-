#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#include <setupapi.h>
#include <initguid.h>
#include <devguid.h>
#include <jansson.h>
#include "logger.h"

// Static helper function declarations
static bool is_target_usb_device(const char* hardware_id);
static ConfigError create_directory_if_not_exists(const char* path);

Config* config_create(void) {
    Config* config = (Config*)malloc(sizeof(Config));
    if (config == NULL) {
        log_error("Failed to allocate memory for config");
        return NULL;
    }

    memset(config, 0, sizeof(Config));
    
    // Set default paths
    strncpy(config->config_path, DEFAULT_CONFIG_PATH, MAX_PATH_LENGTH - 1);
    strncpy(config->log_dir, DEFAULT_LOG_DIR, MAX_PATH_LENGTH - 1);
    strncpy(config->freq_config_path, DEFAULT_FREQ_CONFIG_PATH, MAX_PATH_LENGTH - 1);
    
    config_load_defaults(config);
    log_debug("Config created successfully");
    return config;
}

void config_destroy(Config* config) {
    if (config) {
        log_debug("Destroying config");
        free(config);
    }
}

ConfigError config_load_defaults(Config* config) {
    if (!config) {
        log_error("Cannot load defaults: NULL config");
        return CONFIG_ERROR_MEMORY;
    }

    config->port_auto_detect = true;
    config->is_service = false;
    config->debug_enabled = false;
    config->info_enabled = true;
    config->debug_events = false;
    config->sample_count = 1600;
    config->is_initialized = false;

    log_debug("Default configuration loaded");
    return CONFIG_SUCCESS;
}

ConfigError config_load_from_file(Config* config, const char* config_path) {
    if (!config) {
        log_error("Cannot load config: NULL config");
        return CONFIG_ERROR_MEMORY;
    }
    
    json_error_t error;
    json_t* root = json_load_file(config_path, 0, &error);
    if (!root) {
        log_error("Failed to load config file: %s", error.text);
        return CONFIG_ERROR_JSON_PARSE;
    }
    
    // Load all configuration values
    json_t* value;
    
    // Port configuration
    value = json_object_get(root, "port_name");
    if (json_is_string(value)) {
        strncpy(config->port_name, json_string_value(value), MAX_PORT_NAME_LENGTH - 1);
        log_debug("Loaded port name: %s", config->port_name);
    }
    
    value = json_object_get(root, "port_auto_detect");
    if (json_is_boolean(value)) {
        config->port_auto_detect = json_boolean_value(value);
        log_debug("Port auto-detect: %s", config->port_auto_detect ? "enabled" : "disabled");
    }
    
    // Operation mode
    value = json_object_get(root, "is_service");
    if (json_is_boolean(value)) {
        config->is_service = json_boolean_value(value);
        log_debug("Service mode: %s", config->is_service ? "enabled" : "disabled");
    }
    
    value = json_object_get(root, "debug_enabled");
    if (json_is_boolean(value)) {
        config->debug_enabled = json_boolean_value(value);
        log_debug("Debug mode: %s", config->debug_enabled ? "enabled" : "disabled");
    }
    
    value = json_object_get(root, "info_enabled");
    if (json_is_boolean(value)) {
        config->info_enabled = json_boolean_value(value);
        log_debug("Info logging: %s", config->info_enabled ? "enabled" : "disabled");
    }
    
    value = json_object_get(root, "debug_events");
    if (json_is_boolean(value)) {
        config->debug_events = json_boolean_value(value);
        log_debug("Debug events: %s", config->debug_events ? "enabled" : "disabled");
    }
    
    // Sample configuration
    value = json_object_get(root, "sample_count");
    if (json_is_integer(value)) {
        config->sample_count = (int)json_integer_value(value);
        log_debug("Sample count: %d", config->sample_count);
    }
    
    json_decref(root);
    
    // Validate loaded configuration
    return config_validate(config);
}

ConfigError config_save_to_file(Config* config, const char* config_path) {
    if (!config) {
        log_error("Cannot save config: NULL config");
        return CONFIG_ERROR_MEMORY;
    }
    
    json_t* root = json_object();
    
    // Save all configuration values
    json_object_set_new(root, "port_name", json_string(config->port_name));
    json_object_set_new(root, "port_auto_detect", json_boolean(config->port_auto_detect));
    json_object_set_new(root, "is_service", json_boolean(config->is_service));
    json_object_set_new(root, "debug_enabled", json_boolean(config->debug_enabled));
    json_object_set_new(root, "info_enabled", json_boolean(config->info_enabled));
    json_object_set_new(root, "debug_events", json_boolean(config->debug_events));
    json_object_set_new(root, "sample_count", json_integer(config->sample_count));
    
    if (json_dump_file(root, config_path, JSON_INDENT(2)) != 0) {
        log_error("Failed to save config file: %s", config_path);
        json_decref(root);
        return CONFIG_ERROR_FILE_ACCESS;
    }
    
    log_debug("Configuration saved successfully to: %s", config_path);
    json_decref(root);
    return CONFIG_SUCCESS;
}

ConfigError config_validate(Config* config) {
    if (!config) {
        log_error("Cannot validate config: NULL config");
        return CONFIG_ERROR_MEMORY;
    }
    
    // Validate paths
    if (strlen(config->log_dir) == 0) {
        log_error("Log directory path is empty");
        return CONFIG_ERROR_INVALID_CONFIG;
    }
    
    // Validate port settings
    if (!config->port_auto_detect && strlen(config->port_name) == 0) {
        log_error("Port name is required when auto-detect is disabled");
        return CONFIG_ERROR_INVALID_CONFIG;
    }
    
    // Validate sample count
    if (config->sample_count <= 0 || config->sample_count > MAX_BUFFER_SIZE) {
        log_error("Invalid sample count: %d", config->sample_count);
        return CONFIG_ERROR_INVALID_CONFIG;
    }
    
    log_debug("Configuration validated successfully");
    return CONFIG_SUCCESS;
}

ConfigError config_ensure_directories(Config* config) {
    if (!config) {
        log_error("Cannot ensure directories: NULL config");
        return CONFIG_ERROR_MEMORY;
    }
    
    // Create log directory
    ConfigError result = create_directory_if_not_exists(config->log_dir);
    if (result != CONFIG_SUCCESS) {
        return result;
    }
    
    // Create frequency config directory
    char freq_dir[MAX_PATH_LENGTH];
    strncpy(freq_dir, config->freq_config_path, MAX_PATH_LENGTH - 1);
    char* last_sep = strrchr(freq_dir, '\\');
    if (last_sep) {
        *last_sep = '\0';
        result = create_directory_if_not_exists(freq_dir);
        if (result != CONFIG_SUCCESS) {
            return result;
        }
    }
    
    log_debug("All required directories created/verified");
    return CONFIG_SUCCESS;
}

ConfigError config_detect_port(Config* config) {
    if (!config) {
        log_error("Cannot detect port: NULL config");
        return CONFIG_ERROR_MEMORY;
    }

    HDEVINFO device_info_set = SetupDiGetClassDevs(&GUID_DEVCLASS_PORTS, NULL, NULL, DIGCF_PRESENT);
    if (device_info_set == INVALID_HANDLE_VALUE) {
        log_error("Failed to get device information set");
        return CONFIG_ERROR_PORT_ACCESS;
    }

    SP_DEVINFO_DATA device_info_data;
    device_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

    bool found_port = false;
    for (DWORD i = 0; SetupDiEnumDeviceInfo(device_info_set, i, &device_info_data); i++) {
        char hardware_id[256] = {0};
        if (SetupDiGetDeviceRegistryProperty(device_info_set, &device_info_data, SPDRP_HARDWAREID,
            NULL, (PBYTE)hardware_id, sizeof(hardware_id), NULL)) {
            
            if (is_target_usb_device(hardware_id)) {
                char port_name[MAX_PORT_NAME_LENGTH] = {0};
                HKEY key = SetupDiOpenDevRegKey(device_info_set, &device_info_data,
                    DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
                    
                if (key != INVALID_HANDLE_VALUE) {
                    DWORD type = 0;
                    DWORD size = sizeof(port_name);
                    if (RegQueryValueEx(key, "PortName", NULL, &type, (LPBYTE)port_name, &size) == ERROR_SUCCESS) {
                        strncpy(config->port_name, port_name, MAX_PORT_NAME_LENGTH - 1);
                        found_port = true;
                        log_debug("Found compatible port: %s", port_name);
                    }
                    RegCloseKey(key);
                }
                break;
            }
        }
    }

    SetupDiDestroyDeviceInfoList(device_info_set);

    if (!found_port) {
        log_error("No compatible USB ports found");
        return CONFIG_ERROR_NO_PORTS;
    }

    return CONFIG_SUCCESS;
}

static bool is_target_usb_device(const char* hardware_id) {
    // Check for specific USB vendor and product IDs
    const char* target_ids[] = {
        "USB\\VID_0483&PID_5740",  // Example ID - adjust as needed
        NULL
    };

    for (const char** id = target_ids; *id != NULL; id++) {
        if (strstr(hardware_id, *id) != NULL) {
            log_debug("Found matching USB device ID: %s", hardware_id);
            return true;
        }
    }

    return false;
}

static ConfigError create_directory_if_not_exists(const char* path) {
    if (_mkdir(path) != 0 && errno != EEXIST) {
        log_error("Failed to create directory: %s", path);
        return CONFIG_ERROR_FILE_ACCESS;
    }
    log_debug("Directory created/verified: %s", path);
    return CONFIG_SUCCESS;
}

const char* config_get_error_string(ConfigError error) {
    switch (error) {
        case CONFIG_SUCCESS: return "Success";
        case CONFIG_ERROR_NO_PORTS: return "No compatible ports found";
        case CONFIG_ERROR_PORT_ACCESS: return "Cannot access port";
        case CONFIG_ERROR_LOG_ACCESS: return "Cannot access log file";
        case CONFIG_ERROR_MEMORY: return "Memory error";
        case CONFIG_ERROR_FILE_ACCESS: return "File access error";
        case CONFIG_ERROR_INVALID_CONFIG: return "Invalid configuration";
        case CONFIG_ERROR_JSON_PARSE: return "JSON parse error";
        default: return "Unknown error";
    }
}