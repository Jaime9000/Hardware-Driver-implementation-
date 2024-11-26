#include "commands.h"
#include "serial_interface.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Static variable to store the last error code
static ErrorCode last_error = ERROR_NONE;

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
        {MODE_0, EMG_CONFIG_RAW, true, true, 1600},
        {MODE_42, EMG_CONFIG_RAW, true, true, 1600},
        {MODE_43, EMG_CONFIG_RAW, true, true, 1600},
        {MODE_44, EMG_CONFIG_RAW, true, true, 1600},
        {MODE_51, EMG_CONFIG_RAW, true, true, 1600},
        {MODE_52, EMG_CONFIG_RAW, true, true, 1600},
        {MODE_53, EMG_CONFIG_RAW, true, true, 1600},
        {MODE_56, EMG_CONFIG_RAW, true, true, 1600},
        {MODE_57, EMG_CONFIG_RAW, true, true, 1600},
        {MODE_118, EMG_CONFIG_RAW, true, true, 4},
    };

    for (size_t i = 0; i < sizeof(mode_configs) / sizeof(ModeConfig); ++i) {
        if (strcmp(command_string, get_mode_command_string(mode_configs[i].mode_number)) == 0) {
            return &mode_configs[i];
        }
    }
    return NULL;
}

// Function to validate if a command is a valid mode command
bool validate_mode_command(const char* command) {
    return get_mode_config(command) != NULL;
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
        *io_cmd = 0; // No IO command for mode commands
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

// Function to get a string representation of an error code
const char* get_error_string(ErrorCode code) {
    switch (code) {
        case ERROR_NONE: return "No error";
        case ERROR_INVALID_COMMAND: return "Invalid command";
        case ERROR_HANDSHAKE_FAILED: return "Handshake failed";
        case ERROR_DEVICE_DISCONNECTED: return "Device disconnected";
        case ERROR_TIMEOUT: return "Timeout occurred";
        case ERROR_WRITE_FAILED: return "Write operation failed";
        case ERROR_READ_FAILED: return "Read operation failed";
        case ERROR_BUFFER_OVERFLOW: return "Buffer overflow";
        case ERROR_INVALID_MODE: return "Invalid mode";
        default: return "Unknown error";
    }
}

// Function to set the last error code
void set_last_error(ErrorCode code) {
    last_error = code;
}

// Function to get the last error code
ErrorCode get_last_error(void) {
    return last_error;
}