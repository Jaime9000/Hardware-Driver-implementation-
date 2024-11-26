#ifndef MYOTRONICS_SERIAL_INTERFACE_H
#define MYOTRONICS_SERIAL_INTERFACE_H

#include <windows.h>
#include <stdbool.h>
#include "commands.h"

#define SLOW_BAUD_RATE 115200
#define FAST_BAUD_RATE 230400
#define DEFAULT_TIMEOUT 5000  // 5 seconds
#define MAX_BUFFER_SIZE 32000

// Forward declaration of Config structure (defined in config.h)
typedef struct Config Config;

typedef struct {
    HANDLE handle;
    CRITICAL_SECTION mutex;
    Config* config;
    bool is_connected;
    char* port_name;
    DWORD baud_rate;
    bool handshake_established;
} SerialInterface;

// Constructor/Destructor
SerialInterface* serial_interface_create(Config* config);
void serial_interface_destroy(SerialInterface* interface);

// Basic serial operations
ErrorCode serial_interface_open(SerialInterface* interface);
ErrorCode serial_interface_close(SerialInterface* interface);
ErrorCode serial_interface_write_data(SerialInterface* interface, const unsigned char* data, size_t length);
ErrorCode serial_interface_read_data(SerialInterface* interface, unsigned char* buffer, size_t* length, size_t max_length);
ErrorCode serial_interface_flush(SerialInterface* interface);

// USB Control functions
ErrorCode serial_interface_usb_control_on(SerialInterface* interface);
ErrorCode serial_interface_usb_control_off(SerialInterface* interface);
ErrorCode serial_interface_usb_data_on(SerialInterface* interface);
ErrorCode serial_interface_usb_data_off(SerialInterface* interface);

// Hardware control functions
ErrorCode serial_interface_reset_hardware(SerialInterface* interface, bool is_60hz);
ErrorCode serial_interface_control_statuses(SerialInterface* interface);
ErrorCode serial_interface_get_emg_version(SerialInterface* interface);
ErrorCode serial_interface_check_connection(SerialInterface* interface);
ErrorCode serial_interface_get_equipment_byte(SerialInterface* interface);

// Buffer management
ErrorCode serial_interface_reset_buffers(SerialInterface* interface);

// Handshake functions
ErrorCode serial_interface_perform_handshake(SerialInterface* interface, const char* handshake_string);
bool serial_interface_is_handshake_established(SerialInterface* interface);

#endif // MYOTRONICS_SERIAL_INTERFACE_H