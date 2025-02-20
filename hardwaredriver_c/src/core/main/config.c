#include "src/core/main/config.h"

// System headers needed for implementation
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#include <setupapi.h>
#include <initguid.h>
#include <devguid.h>
#include <errno.h>

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
    
    // Read file
    FILE* file = fopen(config_path, "r");
    if (!file) {
        log_error("Could not open config file: %s", config_path);
        return CONFIG_ERROR_FILE_ACCESS;
    }

    // Get file size and read contents
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* json_string = (char*)malloc(size + 1);
    if (!json_string) {
        fclose(file);
        return CONFIG_ERROR_MEMORY;
    }

    fread(json_string, 1, size, file);
    json_string[size] = '\0';
    fclose(file);

    // Parse JSON
    cJSON* root = cJSON_Parse(json_string);
    free(json_string);

    if (!root) {
        log_error("Failed to parse config JSON");
        return CONFIG_ERROR_JSON_PARSE;
    }

    // Extract values
    cJSON* com_port = cJSON_GetObjectItem(root, "com_port");
    cJSON* auto_detect = cJSON_GetObjectItem(root, "port_auto_detect");
    cJSON* baud_rate = cJSON_GetObjectItem(root, "baud_rate");
    cJSON* log_level = cJSON_GetObjectItem(root, "log_level");
    cJSON* debug_enabled = cJSON_GetObjectItem(root, "debug_enabled");
    cJSON* info_enabled = cJSON_GetObjectItem(root, "info_enabled");
    cJSON* sample_count = cJSON_GetObjectItem(root, "sample_count");

    if (!com_port || !cJSON_IsString(com_port) ||
        !auto_detect || !cJSON_IsBool(auto_detect) ||
        !baud_rate || !cJSON_IsNumber(baud_rate) ||
        !log_level || !cJSON_IsNumber(log_level) ||
        !debug_enabled || !cJSON_IsBool(debug_enabled) ||
        !info_enabled || !cJSON_IsBool(info_enabled) ||
        !sample_count || !cJSON_IsNumber(sample_count)) {
        cJSON_Delete(root);
        return CONFIG_ERROR_JSON_PARSE;
    }

    strncpy(config->com_port, com_port->valuestring, sizeof(config->com_port) - 1);
    config->com_port[sizeof(config->com_port) - 1] = '\0';
    config->port_auto_detect = cJSON_IsTrue(auto_detect);
    config->baud_rate = (DWORD)baud_rate->valuedouble;
    config->log_level = (LogLevel)log_level->valueint;
    config->debug_enabled = cJSON_IsTrue(debug_enabled);
    config->info_enabled = cJSON_IsTrue(info_enabled);
    config->sample_count = (int)sample_count->valuedouble;

    cJSON_Delete(root);
    return config_validate(config);
}

ConfigError config_save_to_file(Config* config, const char* config_path) {
    if (!config) {
        log_error("Cannot save config: NULL config");
        return CONFIG_ERROR_MEMORY;
    }
    
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        return CONFIG_ERROR_MEMORY;
    }

    cJSON_AddStringToObject(root, "com_port", config->com_port);
    cJSON_AddNumberToObject(root, "baud_rate", config->baud_rate);
    cJSON_AddNumberToObject(root, "log_level", config->log_level);

    char* json_string = cJSON_Print(root);
    if (!json_string) {
        cJSON_Delete(root);
        return CONFIG_ERROR_MEMORY;
    }

    FILE* file = fopen(config_path, "w");
    if (!file) {
        free(json_string);
        cJSON_Delete(root);
        return CONFIG_ERROR_FILE_ACCESS;
    }

    fprintf(file, "%s", json_string);
    fclose(file);

    free(json_string);
    cJSON_Delete(root);
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
    if (!config->port_auto_detect && strlen(config->com_port) == 0) {
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
                        strncpy(config->com_port, port_name, MAX_PORT_NAME_LENGTH - 1);
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