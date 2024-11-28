#ifndef CONTROL_FUNCTIONS_H
#define CONTROL_FUNCTIONS_H

#include <stdbool.h>
#include "serial_interface.h"
#include "error_codes.h"
#include "logger.h"  // Add logger include

// Control function structure
typedef struct {
    SerialInterface* serial_interface;
} ControlFunctions;

// Constructor/Destructor
ErrorCode control_functions_init(ControlFunctions* control, SerialInterface* serial_interface);
void control_functions_destroy(ControlFunctions* control);

// USB Data Control
ErrorCode control_functions_usb_data_on(ControlFunctions* control, bool disconnected);
ErrorCode control_functions_usb_data_off(ControlFunctions* control, bool disconnected);

// USB Control Lines
ErrorCode control_functions_usb_control_on(ControlFunctions* control, bool disconnected);
ErrorCode control_functions_usb_control_off(ControlFunctions* control, bool disconnected);

// Hardware Control
ErrorCode control_functions_reset_hardware(ControlFunctions* control, bool is_60hz, bool disconnected);
ErrorCode control_functions_device_statuses(ControlFunctions* control, bool disconnected);

// Command Execution
ErrorCode control_functions_execute(ControlFunctions* control, int command_type, bool disconnected);

#endif // CONTROL_FUNCTIONS_H