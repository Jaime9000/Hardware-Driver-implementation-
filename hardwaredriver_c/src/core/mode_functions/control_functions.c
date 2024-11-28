#include <string.h>
#include "control_functions.h"
#include "logger.h"
#include "commands.h"

ErrorCode control_functions_init(ControlFunctions* control, SerialInterface* serial_interface) {
    if (!control || !serial_interface) {
        set_last_error(ERROR_INVALID_PARAMETER);
        log_error("Invalid parameters in control_functions_init");
        return ERROR_INVALID_PARAMETER;
    }

    control->serial_interface = serial_interface;
    log_debug("Control functions initialized successfully");
    set_last_error(ERROR_NONE);
    return ERROR_NONE;
}

void control_functions_destroy(ControlFunctions* control) {
    if (!control) {
        log_warning("Attempted to destroy NULL control functions");
        return;
    }
    
    log_debug("Destroying control functions");
    memset(control, 0, sizeof(ControlFunctions));
}

ErrorCode control_functions_usb_data_on(ControlFunctions* control, bool disconnected) {
    if (!control || !control->serial_interface) {
        set_last_error(ERROR_INVALID_PARAMETER);
        log_error("Invalid parameters in usb_data_on");
        return ERROR_INVALID_PARAMETER;
    }

    if (disconnected) {
        log_debug("USB data on skipped (disconnected mode)");
        set_last_error(ERROR_NONE);
        return ERROR_NONE;
    }

    log_debug("Setting USB data on");
    ErrorCode result = serial_interface_usb_data_on(control->serial_interface);
    set_last_error(result);
    
    if (result != ERROR_NONE) {
        log_error("Failed to set USB data on: %s", get_error_string(result));
    }
    
    return result;
}

ErrorCode control_functions_usb_data_off(ControlFunctions* control, bool disconnected) {
    if (!control || !control->serial_interface) {
        set_last_error(ERROR_INVALID_PARAMETER);
        log_error("Invalid parameters in usb_data_off");
        return ERROR_INVALID_PARAMETER;
    }

    if (disconnected) {
        log_debug("USB data off skipped (disconnected mode)");
        set_last_error(ERROR_NONE);
        return ERROR_NONE;
    }

    log_debug("Setting USB data off");
    ErrorCode result = serial_interface_usb_data_off(control->serial_interface);
    set_last_error(result);
    
    if (result != ERROR_NONE) {
        log_error("Failed to set USB data off: %s", get_error_string(result));
    }
    
    return result;
}

ErrorCode control_functions_usb_control_on(ControlFunctions* control, bool disconnected) {
    if (!control || !control->serial_interface) {
        set_last_error(ERROR_INVALID_PARAMETER);
        log_error("Invalid parameters in usb_control_on");
        return ERROR_INVALID_PARAMETER;
    }

    if (disconnected) {
        log_debug("USB control on skipped (disconnected mode)");
        set_last_error(ERROR_NONE);
        return ERROR_NONE;
    }

    log_debug("Setting USB control on");
    ErrorCode result = serial_interface_usb_control_on(control->serial_interface);
    set_last_error(result);
    
    if (result != ERROR_NONE) {
        log_error("Failed to set USB control on: %s", get_error_string(result));
    }
    
    return result;
}

ErrorCode control_functions_usb_control_off(ControlFunctions* control, bool disconnected) {
    if (!control || !control->serial_interface) {
        set_last_error(ERROR_INVALID_PARAMETER);
        log_error("Invalid parameters in usb_control_off");
        return ERROR_INVALID_PARAMETER;
    }

    if (disconnected) {
        log_debug("USB control off skipped (disconnected mode)");
        set_last_error(ERROR_NONE);
        return ERROR_NONE;
    }

    log_debug("Setting USB control off");
    ErrorCode result = serial_interface_usb_control_off(control->serial_interface);
    set_last_error(result);
    
    if (result != ERROR_NONE) {
        log_error("Failed to set USB control off: %s", get_error_string(result));
    }
    
    return result;
}

ErrorCode control_functions_reset_hardware(ControlFunctions* control, bool is_60hz, bool disconnected) {
    if (!control || !control->serial_interface) {
        set_last_error(ERROR_INVALID_PARAMETER);
        log_error("Invalid parameters in reset_hardware");
        return ERROR_INVALID_PARAMETER;
    }

    if (disconnected) {
        log_debug("Hardware reset skipped (disconnected mode)");
        set_last_error(ERROR_NONE);
        return ERROR_NONE;
    }

    log_debug("Resetting hardware (60Hz: %s)", is_60hz ? "true" : "false");
    ErrorCode result = serial_interface_reset_hardware(control->serial_interface, is_60hz);
    set_last_error(result);
    
    if (result != ERROR_NONE) {
        log_error("Failed to reset hardware: %s", get_error_string(result));
    }
    
    return result;
}

ErrorCode control_functions_device_statuses(ControlFunctions* control, bool disconnected) {
    if (!control || !control->serial_interface) {
        set_last_error(ERROR_INVALID_PARAMETER);
        log_error("Invalid parameters in device_statuses");
        return ERROR_INVALID_PARAMETER;
    }

    if (disconnected) {
        log_debug("Device statuses skipped (disconnected mode)");
        set_last_error(ERROR_NONE);
        return ERROR_NONE;
    }

    log_debug("Getting device statuses");
    ErrorCode result = serial_interface_control_statuses(control->serial_interface);
    set_last_error(result);
    
    if (result != ERROR_NONE) {
        log_error("Failed to get device statuses: %s", get_error_string(result));
    }
    
    return result;
}

ErrorCode control_functions_execute(ControlFunctions* control, int command_type, bool disconnected) {
    if (!control) {
        set_last_error(ERROR_INVALID_PARAMETER);
        log_error("Invalid control parameter in execute");
        return ERROR_INVALID_PARAMETER;
    }

    log_debug("Executing control function command: %d", command_type);
    ErrorCode result;

    switch (command_type) {
        case CMD_DTR_ON:
            result = control_functions_usb_data_on(control, disconnected);
            break;
            
        case CMD_DTR_OFF:
            result = control_functions_usb_data_off(control, disconnected);
            break;
            
        case CMD_RTS_ON:
            result = control_functions_usb_control_on(control, disconnected);
            break;
            
        case CMD_RTS_OFF:
            result = control_functions_usb_control_off(control, disconnected);
            break;
            
        case CMD_DEVICE_STATUSES:
            result = control_functions_device_statuses(control, disconnected);
            break;
            
        case CMD_RESET_HARDWARE_60:
            result = control_functions_reset_hardware(control, true, disconnected);
            break;
            
        case CMD_RESET_HARDWARE_50:
            result = control_functions_reset_hardware(control, false, disconnected);
            break;
            
        default:
            log_error("Invalid command type: %d", command_type);
            set_last_error(ERROR_INVALID_COMMAND);
            return ERROR_INVALID_COMMAND;
    }

    set_last_error(result);
    if (result != ERROR_NONE) {
        log_error("Command execution failed: %s", get_error_string(result));
    } else {
        log_debug("Command executed successfully");
    }
    
    return result;
}