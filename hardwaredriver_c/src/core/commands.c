#include "src/core/commands.h"
#include "src/core/serial_interface.h"
#include "src/core/error_codes.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


// Helper function to get the string representation of a mode command
const char* get_mode_command_string(ModeNumber mode) {
    switch (mode) {
        case MODE_0: return CMD_MODE_0_CONF;
        case MODE_42: return CMD_MODE_42_RAW;
        case MODE_43: return CMD_MODE_43_RAW;
        case MODE_44: return CMD_MODE_44_RAW;
        case MODE_51: return CMD_MODE_51_RAW;
        case MODE_52: return CMD_MODE_52_RAW;
        case MODE_53: return CMD_MODE_53_RAW;
        case MODE_56: return CMD_MODE_56_RAW;
        case MODE_57: return CMD_MODE_57_RAW;
        case MODE_118: return CMD_EMG_VERSION;
        default: return NULL;
    }
}

// Function to get the mode configuration based on the command string
ModeConfig* get_mode_config(const char* command_string) {
    static ModeConfig mode_configs[] = {
        // Mode 0 variants
        {MODE_0, EMG_CONFIG_PROCESSED, true, true, 1600},  // Base mode-0
        {MODE_0, EMG_CONFIG_RAW, true, true, 1600},       // mode-0-raw
        {MODE_0, EMG_CONFIG_RAW, true, true, 1600},       // mode-0-align

        // Base modes
        {MODE_42, EMG_CONFIG_RAW, true, true, 1600},
        {MODE_43, EMG_CONFIG_RAW, true, true, 1600},
        
        // Mode 44 variants
        {MODE_44, EMG_CONFIG_RAW, true, true, 1600},          // mode-44-raw
        {MODE_44, EMG_CONFIG_RAW, true, true, 1600},          // mode-44-raw-no-image
        {MODE_44, EMG_CONFIG_PROCESSED, true, true, 1600},    // mode-44-sweep
        
        // Other modes
        {MODE_51, EMG_CONFIG_RAW, true, true, 1600},
        {MODE_52, EMG_CONFIG_RAW, true, true, 1600},
        {MODE_53, EMG_CONFIG_RAW, true, true, 1600},
        {MODE_56, EMG_CONFIG_RAW, true, true, 1600},
        
        // Mode 57 variants
        {MODE_57, EMG_CONFIG_RAW, true, true, 1600},         // mode-57-raw
        {MODE_57, EMG_CONFIG_RAW, true, true, 1600},         // mode-57-raw-no-image
        
        // EMG Version
        {MODE_118, EMG_CONFIG_RAW, true, true, 4},
    };

    // Special handling for equipment byte and lead status
    if (strcmp(command_string, CMD_GET_EQUIPMENT_BYTE) == 0) {
        return &mode_configs[3];  // Use MODE_42 config
    }
    if (strcmp(command_string, CMD_GET_EMG_LEAD_STATUS) == 0) {
        return &mode_configs[4];  // Use MODE_43 config
    }

    // Direct mappings for special variants
    if (strcmp(command_string, CMD_MODE_0_CONF) == 0) return &mode_configs[0];
    if (strcmp(command_string, CMD_MODE_0_RAW) == 0) return &mode_configs[1];
    if (strcmp(command_string, CMD_MODE_0_ALIGN) == 0) return &mode_configs[2];
    if (strcmp(command_string, CMD_MODE_44_RAW_NO_IMAGE) == 0) return &mode_configs[6];
    if (strcmp(command_string, CMD_MODE_44_SWEEP) == 0) return &mode_configs[7];
    if (strcmp(command_string, CMD_MODE_57_RAW_NO_IMAGE) == 0) return &mode_configs[13];

    // Check for mode variants
    if (strncmp(command_string, "mode-42-raw-", 11) == 0) {
        return &mode_configs[3];  // Use MODE_42 config
    }
    if (strncmp(command_string, "mode-43-raw-", 11) == 0) {
        return &mode_configs[4];  // Use MODE_43 config
    }

    // Check base modes
    for (size_t i = 0; i < sizeof(mode_configs) / sizeof(ModeConfig); ++i) {
        if (strcmp(command_string, get_mode_command_string(mode_configs[i].mode_number)) == 0) {
            return &mode_configs[i];
        }
    }

    return NULL;
}

// Function to validate if a command is a valid mode command
bool validate_mode_command(const char* command) {
    // Check if command is NULL or empty
    if (!command || command[0] == '\0') {
        return false;
    }

    // Check special commands
    if (strcmp(command, CMD_GET_EQUIPMENT_BYTE) == 0 ||
        strcmp(command, CMD_GET_EMG_LEAD_STATUS) == 0 ||
        strcmp(command, CMD_CHECK_CONNECTION) == 0) {
        return true;
    }

    // Check mode 0 variants
    if (strcmp(command, CMD_MODE_0_CONF) == 0 ||
        strcmp(command, CMD_MODE_0_RAW) == 0 ||
        strcmp(command, CMD_MODE_0_ALIGN) == 0) {
        return true;
    }

    // Check mode 42 variants
    if (strcmp(command, CMD_MODE_42_RAW) == 0 ||
        strcmp(command, CMD_MODE_42_RAW_Q) == 0 ||
        strcmp(command, CMD_MODE_42_RAW_S) == 0 ||
        strcmp(command, CMD_MODE_42_RAW_U) == 0 ||
        strcmp(command, CMD_MODE_42_RAW_W) == 0 ||
        strcmp(command, CMD_MODE_42_RAW_T) == 0 ||
        strcmp(command, CMD_MODE_42_RAW_V) == 0 ||
        strcmp(command, CMD_MODE_42_RAW_P) == 0 ||
        strcmp(command, CMD_MODE_42_RAW_R) == 0) {
        return true;
    }

    // Check mode 43 variants
    if (strcmp(command, CMD_MODE_43_RAW) == 0 ||
        strcmp(command, CMD_MODE_43_RAW_Q) == 0 ||
        strcmp(command, CMD_MODE_43_RAW_S) == 0 ||
        strcmp(command, CMD_MODE_43_RAW_U) == 0 ||
        strcmp(command, CMD_MODE_43_RAW_W) == 0 ||
        strcmp(command, CMD_MODE_43_RAW_T) == 0 ||
        strcmp(command, CMD_MODE_43_RAW_V) == 0 ||
        strcmp(command, CMD_MODE_43_RAW_P) == 0 ||
        strcmp(command, CMD_MODE_43_RAW_R) == 0 ||
        strcmp(command, CMD_MODE_43_EMG) == 0) {
        return true;
    }

    // Check mode 44 variants
    if (strcmp(command, CMD_MODE_44_RAW) == 0 ||
        strcmp(command, CMD_MODE_44_RAW_NO_IMAGE) == 0 ||
        strcmp(command, CMD_MODE_44_SWEEP) == 0) {
        return true;
    }

    // Check other modes
    if (strcmp(command, CMD_MODE_51_RAW) == 0 ||
        strcmp(command, CMD_MODE_52_RAW) == 0 ||
        strcmp(command, CMD_MODE_53_RAW) == 0 ||
        strcmp(command, CMD_MODE_56_RAW) == 0 ||
        strcmp(command, CMD_MODE_57_RAW) == 0 ||
        strcmp(command, CMD_MODE_57_RAW_NO_IMAGE) == 0 ||
        strcmp(command, CMD_EMG_VERSION) == 0) {
        return true;
    }

    return false;
}

// Function to validate if a command is a valid control command
bool validate_control_command(const char* command) {
    return IS_CONTROL_COMMAND(command);
}

// Function to parse a command and determine its type
ErrorCode parse_command(const char* command, IOCommand* io_cmd, ModeConfig** mode_cfg) {
    if (!command || !io_cmd || !mode_cfg) {
        set_last_error(ERROR_INVALID_COMMAND);
        return ERROR_INVALID_COMMAND;
    }

    if (validate_control_command(command)) {
        *io_cmd = (IOCommand)command[0];
        *mode_cfg = NULL;
        return ERROR_NONE;
    } 
    else if (validate_mode_command(command)) {
        *mode_cfg = get_mode_config(command);
        *io_cmd = NULL; // No IO command for mode commands
        return ERROR_NONE;
    }

    set_last_error(ERROR_INVALID_COMMAND);
    return ERROR_INVALID_COMMAND;
}

// Function to execute a command based on its type
ErrorCode execute_command(IOCommand command, SerialInterface* serial_interface) {
    ErrorCode result;

    if (!serial_interface) {
        set_last_error(ERROR_DEVICE_DISCONNECTED);
        return ERROR_DEVICE_DISCONNECTED;
    }

    switch (command) {
        case RTS_ON:
            result = serial_interface_usb_control_on(serial_interface);
            break;
            
        case RTS_OFF:
            result = serial_interface_usb_control_off(serial_interface);
            break;
            
        case DTR_ON:
            result = serial_interface_usb_data_on(serial_interface);
            break;
            
        case DTR_OFF:
            result = serial_interface_usb_data_off(serial_interface);
            break;
            
        case RESET_HARDWARE_60:
            result = serial_interface_reset_hardware(serial_interface, true);  // true for 60Hz
            break;
            
        case RESET_HARDWARE_50:
            result = serial_interface_reset_hardware(serial_interface, false); // false for 50Hz
            break;
            
        case DEVICE_STATUSES:
            result = serial_interface_control_statuses(serial_interface);
            break;
            
        case GET_EMG_VERSION:
            result = serial_interface_get_emg_version(serial_interface);
            break;
            
        case CHECK_DEVICE_CONNECTION:
            result = serial_interface_check_connection(serial_interface);
            break;
            
        case GET_EQUIPMENT_BYTE:
            result = serial_interface_get_equipment_byte(serial_interface);
            break;
            
        default:
            set_last_error(ERROR_INVALID_COMMAND);
            return ERROR_INVALID_COMMAND;
    }

    set_last_error(result);
    return result;
}

// Function to execute a mode command
ErrorCode execute_mode_command(ModeConfig* mode_cfg, SerialInterface* serial_interface, bool disconnected) {
    if (!mode_cfg || !serial_interface) {
        set_last_error(ERROR_INVALID_MODE);
        return ERROR_INVALID_MODE;
    }

    // If disconnected and mode supports it, return dummy data
    if (disconnected && mode_cfg->supports_disconnected) {
        return ERROR_NONE;
    }

    // Check if handshake is required and perform it if needed
    if (mode_cfg->requires_handshake && !serial_interface_is_handshake_established(serial_interface)) {
        ErrorCode result = serial_interface_perform_handshake(serial_interface, 
            mode_cfg->mode_number == MODE_118 ? "K7-MYO6" : "K7-MYO5");
        if (result != ERROR_NONE) {
            set_last_error(result);
            return result;
        }
    }

    // Read data based on mode configuration
    unsigned char buffer[MAX_BUFFER_SIZE];
    size_t bytes_read;
    ErrorCode result = serial_interface_read_data(serial_interface, buffer, &bytes_read, 
                                                mode_cfg->default_byte_count);

    set_last_error(result);
    return result;
}
