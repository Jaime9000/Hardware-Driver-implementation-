#include "serial_interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Helper function declarations
static ErrorCode configure_serial_port(SerialInterface* interface);
static ErrorCode set_timeouts(SerialInterface* interface);

SerialInterface* serial_interface_create(Config* config) {
    SerialInterface* interface = (SerialInterface*)malloc(sizeof(SerialInterface));
    if (!interface) {
        return NULL;
    }

    interface->handle = INVALID_HANDLE_VALUE;
    InitializeCriticalSection(&interface->mutex);
    interface->config = config;
    interface->is_connected = false;
    interface->handshake_established = false;
    interface->baud_rate = FAST_BAUD_RATE;
    interface->port_name = NULL;

    return interface;
}

void serial_interface_destroy(SerialInterface* interface) {
    if (interface) {
        serial_interface_close(interface);
        DeleteCriticalSection(&interface->mutex);
        free(interface->port_name);
        free(interface);
    }
}

ErrorCode serial_interface_open(SerialInterface* interface) {
    EnterCriticalSection(&interface->mutex);
    
    interface->handle = CreateFile(interface->port_name,
                                 GENERIC_READ | GENERIC_WRITE,
                                 0,
                                 NULL,
                                 OPEN_EXISTING,
                                 0,
                                 NULL);

    if (interface->handle == INVALID_HANDLE_VALUE) {
        LeaveCriticalSection(&interface->mutex);
        return ERROR_DEVICE_DISCONNECTED;
    }

    ErrorCode error = configure_serial_port(interface);
    if (error != ERROR_NONE) {
        CloseHandle(interface->handle);
        interface->handle = INVALID_HANDLE_VALUE;
        LeaveCriticalSection(&interface->mutex);
        return error;
    }

    interface->is_connected = true;
    LeaveCriticalSection(&interface->mutex);
    return ERROR_NONE;
}

ErrorCode serial_interface_close(SerialInterface* interface) {
    if (!interface || interface->handle == INVALID_HANDLE_VALUE) {
        return ERROR_DEVICE_DISCONNECTED;
    }

    EnterCriticalSection(&interface->mutex);
    CloseHandle(interface->handle);
    interface->handle = INVALID_HANDLE_VALUE;
    interface->is_connected = false;
    interface->handshake_established = false;
    LeaveCriticalSection(&interface->mutex);
    
    return ERROR_NONE;
}

ErrorCode serial_interface_write_data(SerialInterface* interface, const unsigned char* data, size_t length) {
    if (!interface->is_connected) {
        return ERROR_DEVICE_DISCONNECTED;
    }

    EnterCriticalSection(&interface->mutex);
    DWORD bytes_written;
    BOOL success = WriteFile(interface->handle, data, (DWORD)length, &bytes_written, NULL);
    LeaveCriticalSection(&interface->mutex);

    if (!success || bytes_written != length) {
        return ERROR_WRITE_FAILED;
    }

    return ERROR_NONE;
}

ErrorCode serial_interface_read_data(SerialInterface* interface, unsigned char* buffer, size_t* length, size_t max_length) {
    if (!interface->is_connected) {
        return ERROR_DEVICE_DISCONNECTED;
    }

    EnterCriticalSection(&interface->mutex);
    DWORD bytes_read;
    BOOL success = ReadFile(interface->handle, buffer, (DWORD)max_length, &bytes_read, NULL);
    LeaveCriticalSection(&interface->mutex);

    if (!success) {
        *length = 0;
        return ERROR_READ_FAILED;
    }

    *length = bytes_read;
    return ERROR_NONE;
}

// USB Control Functions
ErrorCode serial_interface_usb_control_on(SerialInterface* interface) {
    if (!interface->is_connected) {
        return ERROR_DEVICE_DISCONNECTED;
    }

    if (!EscapeCommFunction(interface->handle, SETRTS)) {
        return ERROR_WRITE_FAILED;
    }
    return ERROR_NONE;
}

ErrorCode serial_interface_usb_control_off(SerialInterface* interface) {
    if (!interface->is_connected) {
        return ERROR_DEVICE_DISCONNECTED;
    }

    if (!EscapeCommFunction(interface->handle, CLRRTS)) {
        return ERROR_WRITE_FAILED;
    }
    return ERROR_NONE;
}

ErrorCode serial_interface_usb_data_on(SerialInterface* interface) {
    if (!interface->is_connected) {
        return ERROR_DEVICE_DISCONNECTED;
    }

    if (!EscapeCommFunction(interface->handle, SETDTR)) {
        return ERROR_WRITE_FAILED;
    }
    return ERROR_NONE;
}

ErrorCode serial_interface_usb_data_off(SerialInterface* interface) {
    if (!interface->is_connected) {
        return ERROR_DEVICE_DISCONNECTED;
    }

    if (!EscapeCommFunction(interface->handle, CLRDTR)) {
        return ERROR_WRITE_FAILED;
    }
    return ERROR_NONE;
}

// Hardware control functions
ErrorCode serial_interface_reset_hardware(SerialInterface* interface, bool is_60hz) {
    const char* handshake_string = is_60hz ? "K7-MYO6" : "K7-MYO5";
    return serial_interface_perform_handshake(interface, handshake_string);
}

ErrorCode serial_interface_perform_handshake(SerialInterface* interface, const char* handshake_string) {
    if (!interface->is_connected) {
        return ERROR_DEVICE_DISCONNECTED;
    }

    // Turn off USB control and data lines
    serial_interface_usb_control_off(interface);
    serial_interface_usb_data_off(interface);
    
    // Reset buffers
    serial_interface_reset_buffers(interface);

    // Turn on USB control and data lines
    serial_interface_usb_control_on(interface);
    serial_interface_usb_data_on(interface);

    // Send handshake string
    ErrorCode error = serial_interface_write_data(interface, 
                                                (const unsigned char*)handshake_string, 
                                                strlen(handshake_string));
    if (error != ERROR_NONE) {
        return error;
    }

    // Read response
    unsigned char buffer[32];
    size_t bytes_read;
    error = serial_interface_read_data(interface, buffer, &bytes_read, sizeof(buffer));
    if (error != ERROR_NONE) {
        return error;
    }

    // Verify handshake response
    if (bytes_read < 7 || memcmp(buffer, "K7-MYO Ver", 7) != 0) {
        return ERROR_HANDSHAKE_FAILED;
    }

    interface->handshake_established = true;
    return ERROR_NONE;
}

// Helper functions implementation
static ErrorCode configure_serial_port(SerialInterface* interface) {
    DCB dcb = {0};
    dcb.DCBlength = sizeof(DCB);

    if (!GetCommState(interface->handle, &dcb)) {
        return ERROR_DEVICE_DISCONNECTED;
    }

    dcb.BaudRate = interface->baud_rate;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;
    dcb.fDtrControl = DTR_CONTROL_DISABLE;

    if (!SetCommState(interface->handle, &dcb)) {
        return ERROR_DEVICE_DISCONNECTED;
    }

    return set_timeouts(interface);
}

static ErrorCode set_timeouts(SerialInterface* interface) {
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = DEFAULT_TIMEOUT;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = DEFAULT_TIMEOUT;
    timeouts.WriteTotalTimeoutMultiplier = 10;

    if (!SetCommTimeouts(interface->handle, &timeouts)) {
        return ERROR_DEVICE_DISCONNECTED;
    }

    return ERROR_NONE;
}

// Additional function implementations...
ErrorCode serial_interface_flush(SerialInterface* interface) {
    if (!interface->is_connected) {
        return ERROR_DEVICE_DISCONNECTED;
    }

    if (!FlushFileBuffers(interface->handle)) {
        return ERROR_WRITE_FAILED;
    }
    return ERROR_NONE;
}

ErrorCode serial_interface_reset_buffers(SerialInterface* interface) {
    if (!interface->is_connected) {
        return ERROR_DEVICE_DISCONNECTED;
    }

    if (!PurgeComm(interface->handle, PURGE_RXCLEAR | PURGE_TXCLEAR)) {
        return ERROR_WRITE_FAILED;
    }
    return ERROR_NONE;
}