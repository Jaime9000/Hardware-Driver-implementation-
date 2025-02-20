#include "src/core/serial_interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cJSON.h>

// Helper function declarations
static ErrorCode configure_serial_port(SerialInterface*serial_interface);
static ErrorCode set_timeouts(SerialInterface*serial_interface);
static ErrorCode perform_handshake_attempt(SerialInterface*serial_interface, const char* handshake_string);
static ErrorCode read_until_empty(SerialInterface*serial_interface);
static ErrorCode validate_interface(SerialInterface*serial_interface);

SerialInterface* serial_interface_create(Config* config) {
    if (!config) {
        log_error("Cannot create serialserial_interface: NULL config");
        return NULL;
    }

    SerialInterface*serial_interface = (SerialInterface*)malloc(sizeof(SerialInterface));
    if (!serial_interface) {
        log_error("Failed to allocate memory for serialserial_interface");
        return NULL;
    }

    // Initialize basic members
   serial_interface->handle = INVALID_HANDLE_VALUE;
   InitializeCriticalSection(&serial_interface->mutex);
   serial_interface->config = config;
   serial_interface->is_connected = false;
   serial_interface->handshake_established = false;
   serial_interface->baud_rate = FAST_BAUD_RATE;
   serial_interface->logger = config->logger;

    // Initialize command maps
   serial_interface->command_maps = NULL;
   serial_interface->command_count = 0;

    // Get port name from config
    const char* port_name = config_get_port_name(config);
    if (!port_name && config->port_auto_detect) {
        ConfigError config_error = config_sense_ports(config);
        if (config_error != CONFIG_SUCCESS) {
            log_error("Failed to auto-detect serial port: %s", config_get_error_string(config_error));
            DeleteCriticalSection(&serial_interface->mutex);
            free(serial_interface);
            return NULL;
        }
        port_name = config_get_port_name(config);
    }

    if (!port_name) {
        log_error("No valid port name available");
        DeleteCriticalSection(&serial_interface->mutex);
        free(serial_interface);
        return NULL;
    }

   serial_interface->port_name = _strdup(port_name);
    if (!serial_interface->port_name) {
        log_error("Failed to allocate memory for port name");
        DeleteCriticalSection(&serial_interface->mutex);
        free(serial_interface);
        return NULL;
    }

    log_debug("Serialserial_interface created successfully for port %s",serial_interface->port_name);
    return serial_interface;
}

void serial_interface_destroy(SerialInterface*serial_interface) {
    if (serial_interface) {
        log_debug("Destroying serialserial_interface");
        EnterCriticalSection(&serial_interface->mutex);
        
        if (serial_interface->is_connected) {
            serial_interface_close(serial_interface);
        }
        
        // Free command maps
        free(serial_interface->command_maps);
        
        LeaveCriticalSection(&serial_interface->mutex);
        DeleteCriticalSection(&serial_interface->mutex);
        
        free(serial_interface->port_name);
        free(serial_interface);
    }
}

ErrorCode serial_interface_open(SerialInterface*serial_interface) {
    if (!serial_interface) {
        log_error("Cannot open nullserial_interface");
        return ERROR_INVALID_PARAM;
    }

    log_info("Opening serial port: %s",serial_interface->port_name);
    EnterCriticalSection(&serial_interface->mutex);

    if (serial_interface->is_connected) {
        log_warning("Port already open, closing first");
        serial_interface_close(serial_interface);
    }

    // Create the file handle with proper flags for serial communication
   serial_interface->handle = CreateFile(serial_interface->port_name,
                                 GENERIC_READ | GENERIC_WRITE,
                                 0,  // No sharing
                                 NULL,
                                 OPEN_EXISTING,
                                 FILE_ATTRIBUTE_NORMAL,
                                 NULL);

    if (serial_interface->handle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        log_error("Failed to open serial port. System error: %lu", error);
        LeaveCriticalSection(&serial_interface->mutex);
        return ERROR_DEVICE_DISCONNECTED;
    }

    // Configure the port
    ErrorCode error = configure_serial_port(serial_interface);
    if (error != ERROR_NONE) {
        log_error("Failed to configure serial port");
        CloseHandle(serial_interface->handle);
       serial_interface->handle = INVALID_HANDLE_VALUE;
        LeaveCriticalSection(&serial_interface->mutex);
        return error;
    }

    // Set the timeouts
    error = set_timeouts(serial_interface);
    if (error != ERROR_NONE) {
        log_error("Failed to set port timeouts");
        CloseHandle(serial_interface->handle);
       serial_interface->handle = INVALID_HANDLE_VALUE;
        LeaveCriticalSection(&serial_interface->mutex);
        return error;
    }

   serial_interface->is_connected = true;
   serial_interface->handshake_established = false;
    log_info("Serial port opened successfully");
    
    LeaveCriticalSection(&serial_interface->mutex);
    return ERROR_NONE;
}

ErrorCode serial_interface_close(SerialInterface*serial_interface) {
    ErrorCode result = validate_interface(serial_interface);
    if (result != ERROR_NONE) {
        return result;
    }

    log_info("Closing serial port");
    EnterCriticalSection(&serial_interface->mutex);

    // Ensure all data is written before closing
    if (serial_interface->is_connected) {
        FlushFileBuffers(serial_interface->handle);
        CloseHandle(serial_interface->handle);
       serial_interface->handle = INVALID_HANDLE_VALUE;
       serial_interface->is_connected = false;
       serial_interface->handshake_established = false;
    }

    LeaveCriticalSection(&serial_interface->mutex);
    return ERROR_NONE;
}

static ErrorCode validate_interface(SerialInterface*serial_interface) {
    if (!serial_interface) {
        log_error("Nullserial_interface pointer");
        return ERROR_INVALID_PARAM;
    }

    // Check if we can actually communicate with the serial port
    DCB dcb = {0};
    if (!GetCommState(serial_interface->handle, &dcb)) {
        log_error("Serial communication not available");
        return ERROR_SERIAL_EXCEPTION;  // Use new error code here
    }

    if (!serial_interface->is_connected) {
        log_error("Device not connected");
        return ERROR_DEVICE_DISCONNECTED;
    }

    if (serial_interface->handle == INVALID_HANDLE_VALUE) {
        log_error("Invalid handle value");
        return ERROR_DEVICE_DISCONNECTED;
    }

    return ERROR_NONE;
}

static ErrorCode configure_serial_port(SerialInterface*serial_interface) {
    DCB dcb = {0};
    dcb.DCBlength = sizeof(DCB);

    if (!GetCommState(serial_interface->handle, &dcb)) {
        DWORD error = GetLastError();
        log_error("Failed to get comm state. System error: %lu", error);
        return ERROR_DEVICE_DISCONNECTED;
    }

    // Configure serial parameters
    dcb.BaudRate =serial_interface->baud_rate;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fDtrControl = DTR_CONTROL_DISABLE;  // DTR disabled initially
    dcb.fRtsControl = RTS_CONTROL_DISABLE;  // RTS disabled initially
    dcb.fOutxCtsFlow = FALSE;  // No CTS flow control
    dcb.fOutxDsrFlow = FALSE;  // No DSR flow control
    dcb.fDsrSensitivity = FALSE;
    dcb.fOutX = FALSE;  // No XON/XOFF out flow control
    dcb.fInX = FALSE;   // No XON/XOFF in flow control
    dcb.fErrorChar = FALSE;
    dcb.fNull = FALSE;
    dcb.fAbortOnError = FALSE;

    if (!SetCommState(serial_interface->handle, &dcb)) {
        DWORD error = GetLastError();
        log_error("Failed to set comm state. System error: %lu", error);
        return ERROR_DEVICE_DISCONNECTED;
    }

    return ERROR_NONE;
}

ErrorCode serial_interface_write_data(SerialInterface*serial_interface, const unsigned char* data, size_t length) {
    ErrorCode result = validate_interface(serial_interface);
    if (result != ERROR_NONE) {
        return result;
    }

    if (!data || length == 0) {
        log_error("Invalid write parameters");
        return ERROR_INVALID_PARAM;
    }

    log_debug("Writing %zu bytes to serial port", length);
    EnterCriticalSection(&serial_interface->mutex);

    DWORD bytes_written;
    BOOL success = WriteFile(serial_interface->handle, data, (DWORD)length, &bytes_written, NULL);
    
    if (!success || bytes_written != length) {
        DWORD error = GetLastError();
        log_error("Write failed. System error: %lu", error);
        LeaveCriticalSection(&serial_interface->mutex);
        return ERROR_WRITE_FAILED;
    }

    // Ensure data is actually sent
    if (!FlushFileBuffers(serial_interface->handle)) {
        DWORD error = GetLastError();
        log_error("Failed to flush buffers. System error: %lu", error);
        LeaveCriticalSection(&serial_interface->mutex);
        return ERROR_WRITE_FAILED;
    }

    LeaveCriticalSection(&serial_interface->mutex);
    log_debug("Successfully wrote %lu bytes", bytes_written);
    return ERROR_NONE;
}

ErrorCode serial_interface_read_data(SerialInterface*serial_interface, unsigned char* buffer, size_t* bytes_read, size_t max_length) {
    ErrorCode result = validate_interface(serial_interface);
    if (result != ERROR_NONE) {
        return result;
    }

    if (!buffer || !bytes_read || max_length == 0) {
        log_error("Invalid read parameters");
        return ERROR_INVALID_PARAM;
    }

    log_debug("Attempting to read up to %zu bytes", max_length);
    EnterCriticalSection(&serial_interface->mutex);

    DWORD bytes_read_local;
    BOOL success = ReadFile(serial_interface->handle, buffer, (DWORD)max_length, &bytes_read_local, NULL);
    
    if (!success) {
        DWORD error = GetLastError();
        log_error("Read failed. System error: %lu", error);
        *bytes_read = 0;
        LeaveCriticalSection(&serial_interface->mutex);
        return ERROR_READ_FAILED;
    }

    *bytes_read = bytes_read_local;
    LeaveCriticalSection(&serial_interface->mutex);
    log_debug("Successfully read %lu bytes", bytes_read_local);
    return ERROR_NONE;
}

ErrorCode serial_interface_flush(SerialInterface*serial_interface) {
    ErrorCode result = validate_interface(serial_interface);
    if (result != ERROR_NONE) {
        return result;
    }

    log_debug("Flushing serial port buffers");
    EnterCriticalSection(&serial_interface->mutex);

    if (!FlushFileBuffers(serial_interface->handle)) {
        DWORD error = GetLastError();
        log_error("Failed to flush buffers. System error: %lu", error);
        LeaveCriticalSection(&serial_interface->mutex);
        return ERROR_WRITE_FAILED;
    }

    LeaveCriticalSection(&serial_interface->mutex);
    return ERROR_NONE;
}

// USB Control Functions
ErrorCode serial_interface_usb_control_on(SerialInterface*serial_interface) {
    ErrorCode result = validate_interface(serial_interface);
    if (result != ERROR_NONE) {
        return result;
    }

    log_debug("Setting RTS control on");
    EnterCriticalSection(&serial_interface->mutex);

    if (!EscapeCommFunction(serial_interface->handle, SETRTS)) {
        DWORD error = GetLastError();
        log_error("Failed to set RTS. System error: %lu", error);
        LeaveCriticalSection(&serial_interface->mutex);
        return ERROR_WRITE_FAILED;
    }

    if (!FlushFileBuffers(serial_interface->handle)) {
        DWORD error = GetLastError();
        log_error("Failed to flush buffers. System error: %lu", error);
        LeaveCriticalSection(&serial_interface->mutex);
        return ERROR_WRITE_FAILED;
    }

    LeaveCriticalSection(&serial_interface->mutex);
    return ERROR_NONE;
}

ErrorCode serial_interface_usb_control_off(SerialInterface*serial_interface) {
    ErrorCode result = validate_interface(serial_interface);
    if (result != ERROR_NONE) {
        return result;
    }

    log_debug("Setting RTS control off");
    EnterCriticalSection(&serial_interface->mutex);

    if (!EscapeCommFunction(serial_interface->handle, CLRRTS)) {
        DWORD error = GetLastError();
        log_error("Failed to clear RTS. System error: %lu", error);
        LeaveCriticalSection(&serial_interface->mutex);
        return ERROR_WRITE_FAILED;
    }

    if (!FlushFileBuffers(serial_interface->handle)) {
        DWORD error = GetLastError();
        log_error("Failed to flush buffers. System error: %lu", error);
        LeaveCriticalSection(&serial_interface->mutex);
        return ERROR_WRITE_FAILED;
    }

    LeaveCriticalSection(&serial_interface->mutex);
    return ERROR_NONE;
}

ErrorCode serial_interface_usb_data_on(SerialInterface*serial_interface) {
    ErrorCode result = validate_interface(serial_interface);
    if (result != ERROR_NONE) {
        return result;
    }

    log_debug("Setting DTR control on");
    EnterCriticalSection(&serial_interface->mutex);

    if (!EscapeCommFunction(serial_interface->handle, SETDTR)) {
        DWORD error = GetLastError();
        log_error("Failed to set DTR. System error: %lu", error);
        LeaveCriticalSection(&serial_interface->mutex);
        return ERROR_WRITE_FAILED;
    }

    if (!FlushFileBuffers(serial_interface->handle)) {
        DWORD error = GetLastError();
        log_error("Failed to flush buffers. System error: %lu", error);
        LeaveCriticalSection(&serial_interface->mutex);
        return ERROR_WRITE_FAILED;
    }

    LeaveCriticalSection(&serial_interface->mutex);
    return ERROR_NONE;
}

ErrorCode serial_interface_usb_data_off(SerialInterface*serial_interface) {
    ErrorCode result = validate_interface(serial_interface);
    if (result != ERROR_NONE) {
        return result;
    }

    log_debug("Setting DTR control off");
    EnterCriticalSection(&serial_interface->mutex);

    if (!EscapeCommFunction(serial_interface->handle, CLRDTR)) {
        DWORD error = GetLastError();
        log_error("Failed to clear DTR. System error: %lu", error);
        LeaveCriticalSection(&serial_interface->mutex);
        return ERROR_WRITE_FAILED;
    }

    if (!FlushFileBuffers(serial_interface->handle)) {
        DWORD error = GetLastError();
        log_error("Failed to flush buffers. System error: %lu", error);
        LeaveCriticalSection(&serial_interface->mutex);
        return ERROR_WRITE_FAILED;
    }

    LeaveCriticalSection(&serial_interface->mutex);
    return ERROR_NONE;
}

ErrorCode serial_interface_reset_hardware(SerialInterface*serial_interface, bool is_60hz) {
    ErrorCode result = validate_interface(serial_interface);
    if (result != ERROR_NONE) {
        return result;
    }

    log_info("Resetting hardware for %dHz operation", is_60hz ? 60 : 50);
    const char* handshake_string = is_60hz ? HANDSHAKE_STRING_60_HZ : HANDSHAKE_STRING_50_HZ;

    // Update frequency config in the system
    result = serial_interface_set_current_freq_config(serial_interface, is_60hz ? 60 : 50);
    if (result != ERROR_NONE) {
        log_error("Failed to update frequency configuration");
        return result;
    }

    return serial_interface_perform_handshake(serial_interface, handshake_string);
}

ErrorCode serial_interface_perform_handshake(SerialInterface*serial_interface, const char* handshake_string) {
    ErrorCode result = validate_interface(serial_interface);
    if (result != ERROR_NONE) {
        return result;
    }

    log_info("Performing handshake with string: %s", handshake_string);
    EnterCriticalSection(&serial_interface->mutex);

    // Reset handshake status
   serial_interface->handshake_established = false;

    // Try handshake multiple times
    for (int attempt = 0; attempt < MAX_HANDSHAKE_ATTEMPTS; attempt++) {
        log_debug("Handshake attempt %d of %d", attempt + 1, MAX_HANDSHAKE_ATTEMPTS);
        
        result = perform_handshake_attempt(serial_interface, handshake_string);
        if (result == ERROR_NONE) {
           serial_interface->handshake_established = true;
            log_info("Handshake successful");
            LeaveCriticalSection(&serial_interface->mutex);
            return ERROR_NONE;
        }
        
        // Small delay between attempts
        Sleep(100);
    }

    log_error("Handshake failed after %d attempts", MAX_HANDSHAKE_ATTEMPTS);
    LeaveCriticalSection(&serial_interface->mutex);
    return ERROR_HANDSHAKE_FAILED;
}

static ErrorCode perform_handshake_attempt(SerialInterface*serial_interface, const char* handshake_string) {
    // Turn off USB control lines
    serial_interface_usb_control_off(serial_interface);
    serial_interface_usb_data_off(serial_interface);
    
    // Clear any pending data
    serial_interface_reset_buffers(serial_interface);
    read_until_empty(serial_interface);

    // Turn on USB control lines
    serial_interface_usb_control_on(serial_interface);
    serial_interface_usb_data_on(serial_interface);
    
    // Send handshake string
    ErrorCode result = serial_interface_write_data(serial_interface, 
        (const unsigned char*)handshake_string, 
        strlen(handshake_string));
    
    if (result != ERROR_NONE) {
        return result;
    }

    // Read response
    unsigned char buffer[MAX_VERSION_STRING_LENGTH];
    size_t bytes_read;
    result = serial_interface_read_data(serial_interface, buffer, &bytes_read, sizeof(buffer) - 1);
    
    if (result != ERROR_NONE) {
        return result;
    }

    // Null terminate and verify response
    buffer[bytes_read] = '\0';
    if (strncmp((char*)buffer, HANDSHAKE_RESPONSE_PREFIX, strlen(HANDSHAKE_RESPONSE_PREFIX)) != 0) {
        return ERROR_HANDSHAKE_FAILED;
    }

    return ERROR_NONE;
}

ErrorCode serial_interface_control_statuses(SerialInterface*serial_interface, char* status_string, bool as_json) {
    ErrorCode result = validate_interface(serial_interface);
    if (result != ERROR_NONE) {
        return result;
    }

    DWORD status;
    if (!GetCommModemStatus(serial_interface->handle, &status)) {
        log_error("Failed to get modem status");
        return ERROR_READ_FAILED;
    }

    bool rts = serial_interface_get_rts_status(serial_interface);
    bool dtr = serial_interface_get_dtr_status(serial_interface);
    bool cts = (status & MS_CTS_ON) != 0;
    bool dsr = (status & MS_DSR_ON) != 0;
    bool cd = (status & MS_RLSD_ON) != 0;
    bool ri = (status & MS_RING_ON) != 0;

    if (as_json) {
        snprintf(status_string, MAX_STATUS_STRING_LENGTH,
                "{\"rts\":%d,\"dtr\":%d,\"cts\":%d,\"dsr\":%d,\"cd\":%d,\"ri\":%d}",
                rts, dtr, cts, dsr, cd, ri);
    } else {
        snprintf(status_string, MAX_STATUS_STRING_LENGTH,
                "%d%d%d%d%d%d", rts, dtr, cts, dsr, cd, ri);
    }

    return ERROR_NONE;
}

ErrorCode serial_interface_get_current_freq_config(SerialInterface*serial_interface, int* frequency) {
    if (!serial_interface || !frequency) {
        return ERROR_INVALID_PARAM;
    }

    FILE* file = fopen(serial_interface->config->freq_config_path, "r");
    if (!file) {
        log_error("Failed to open frequency config file");
        return ERROR_FILE_OPERATION;
    }

    char freq_str[8];
    if (fgets(freq_str, sizeof(freq_str), file) == NULL) {
        fclose(file);
        log_error("Failed to read frequency config");
        return ERROR_FILE_OPERATION;
    }

    fclose(file);
    *frequency = atoi(freq_str);
    
    if (*frequency != 50 && *frequency != 60) {
        log_error("Invalid frequency value: %d", *frequency);
        return ERROR_INVALID_CONFIG;
    }

    return ERROR_NONE;
}

ErrorCode serial_interface_set_current_freq_config(SerialInterface*serial_interface, int frequency) {
    if (!serial_interface) {
        return ERROR_INVALID_PARAM;
    }

    if (frequency != 50 && frequency != 60) {
        log_error("Invalid frequency value: %d", frequency);
        return ERROR_INVALID_PARAM;
    }

    FILE* file = fopen(serial_interface->config->freq_config_path, "w");
    if (!file) {
        log_error("Failed to open frequency config file for writing");
        return ERROR_FILE_OPERATION;
    }

    if (fprintf(file, "%d", frequency) < 0) {
        fclose(file);
        log_error("Failed to write frequency config");
        return ERROR_FILE_OPERATION;
    }

    fclose(file);
    return ERROR_NONE;
}

// Status getter functions
bool serial_interface_get_rts_status(SerialInterface*serial_interface) {
    if (!serial_interface || !serial_interface->is_connected) {
        return false;
    }

    DWORD modemStatus;
    if (GetCommModemStatus(serial_interface->handle, &modemStatus)) {
        return (modemStatus & MS_RTS_ON) != 0;
    }
    return false;
}

bool serial_interface_get_dtr_status(SerialInterface*serial_interface) {
    if (!serial_interface || !serial_interface->is_connected) {
        return false;
    }

    DWORD modemStatus;
    if (GetCommModemStatus(serial_interface->handle, &modemStatus)) {
        return (modemStatus & MS_DTR_ON) != 0;
    }
    return false;
}

bool serial_interface_get_cts_status(SerialInterface*serial_interface) {
    if (!serial_interface || !serial_interface->is_connected) {
        return false;
    }

    DWORD modemStatus;
    if (GetCommModemStatus(serial_interface->handle, &modemStatus)) {
        return (modemStatus & MS_CTS_ON) != 0;
    }
    return false;
}

bool serial_interface_get_dsr_status(SerialInterface*serial_interface) {
    if (!serial_interface || !serial_interface->is_connected) {
        return false;
    }

    DWORD modemStatus;
    if (GetCommModemStatus(serial_interface->handle, &modemStatus)) {
        return (modemStatus & MS_DSR_ON) != 0;
    }
    return false;
}

bool serial_interface_get_cd_status(SerialInterface*serial_interface) {
    if (!serial_interface || !serial_interface->is_connected) {
        return false;
    }

    DWORD modemStatus;
    if (GetCommModemStatus(serial_interface->handle, &modemStatus)) {
        return (modemStatus & MS_RLSD_ON) != 0;
    }
    return false;
}

bool serial_interface_get_ri_status(SerialInterface*serial_interface) {
    if (!serial_interface || !serial_interface->is_connected) {
        return false;
    }

    DWORD modemStatus;
    if (GetCommModemStatus(serial_interface->handle, &modemStatus)) {
        return (modemStatus & MS_RING_ON) != 0;
    }
    return false;
}

bool serial_interface_is_handshake_established(SerialInterface*serial_interface) {
    returnserial_interface &&serial_interface->handshake_established;
}

// Buffer management
ErrorCode serial_interface_reset_buffers(SerialInterface*serial_interface) {
    ErrorCode result = validate_interface(serial_interface);
    if (result != ERROR_NONE) {
        return result;
    }

    log_debug("Resetting serial port buffers");
    EnterCriticalSection(&serial_interface->mutex);

    if (!PurgeComm(serial_interface->handle, 
                   PURGE_RXCLEAR | PURGE_TXCLEAR | 
                   PURGE_RXABORT | PURGE_TXABORT)) {
        DWORD error = GetLastError();
        log_error("Failed to purge comm buffers. System error: %lu", error);
        LeaveCriticalSection(&serial_interface->mutex);
        return ERROR_WRITE_FAILED;
    }

    LeaveCriticalSection(&serial_interface->mutex);
    return ERROR_NONE;
}

// Helper function to read until no more data is available
static ErrorCode read_until_empty(SerialInterface*serial_interface) {
    unsigned char buffer[256];
    size_t bytes_read;
    
    while (true) {
        ErrorCode result = serial_interface_read_data(serial_interface, buffer, &bytes_read, sizeof(buffer));
        if (result != ERROR_NONE || bytes_read == 0) {
            break;
        }
    }
    
    return ERROR_NONE;
}

// Timeout configuration
static ErrorCode set_timeouts(SerialInterface*serial_interface) {
    COMMTIMEOUTS timeouts = {0};
    
    // Configure read timeouts
    timeouts.ReadIntervalTimeout = MAXDWORD;  // Return immediately if no data
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = DEFAULT_TIMEOUT;
    
    // Configure write timeouts
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = DEFAULT_TIMEOUT;

    if (!SetCommTimeouts(serial_interface->handle, &timeouts)) {
        DWORD error = GetLastError();
        log_error("Failed to set comm timeouts. System error: %lu", error);
        return ERROR_DEVICE_DISCONNECTED;
    }

    return ERROR_NONE;
}

// EMG Version and Equipment byte functions
ErrorCode serial_interface_get_emg_version(SerialInterface*serial_interface, char* version_string, size_t max_length) {
    ErrorCode result = validate_interface(serial_interface);
    if (result != ERROR_NONE) {
        return result;
    }

    if (!version_string || max_length == 0) {
        return ERROR_INVALID_PARAM;
    }

    // Send version request command
    const char* cmd = "get-emg-version";
    result = serial_interface_write_data(serial_interface, (const unsigned char*)cmd, strlen(cmd));
    if (result != ERROR_NONE) {
        return result;
    }

    // Read response
    unsigned char buffer[MAX_VERSION_STRING_LENGTH];
    size_t bytes_read;
    result = serial_interface_read_data(serial_interface, buffer, &bytes_read, sizeof(buffer) - 1);
    
    if (result != ERROR_NONE) {
        return result;
    }

    // Ensure null termination and copy to output
    buffer[bytes_read] = '\0';
    strncpy(version_string, (char*)buffer, max_length - 1);
    version_string[max_length - 1] = '\0';

    return ERROR_NONE;
}

ErrorCode serial_interface_get_equipment_byte(SerialInterface*serial_interface, unsigned char* equipment_byte) {
    ErrorCode result = validate_interface(serial_interface);
    if (result != ERROR_NONE) {
        return result;
    }

    if (!equipment_byte) {
        return ERROR_INVALID_PARAM;
    }

    // Send equipment byte request command
    const char* cmd = "get-equipment-byte";
    result = serial_interface_write_data(serial_interface, (const unsigned char*)cmd, strlen(cmd));
    if (result != ERROR_NONE) {
        return result;
    }

    // Read response
    size_t bytes_read;
    result = serial_interface_read_data(serial_interface, equipment_byte, &bytes_read, 1);
    
    if (result != ERROR_NONE || bytes_read != 1) {
        return ERROR_READ_FAILED;
    }

    return ERROR_NONE;
}

ErrorCode serial_interface_check_connection(SerialInterface*serial_interface) {
    return validate_interface(serial_interface);
}

ErrorCode serial_interface_execute_command(SerialInterface*serial_interface, 
                                         const uint8_t* command,
                                         size_t command_size,
                                         ModeManager* mode_manager) {
    if (!serial_interface || !command || !mode_manager) {
        set_last_error(ERROR_INVALID_PARAM);
        return ERROR_INVALID_PARAM;
    }

    // Parse command and return size
    size_t return_size = 0;
    IOCommand cmd;
    if (parse_command_and_size(command, command_size, &cmd, &return_size) != ERROR_NONE) {
        return ERROR_INVALID_COMMAND;
    }

    EnterCriticalSection(&serial_interface->mutex);

    // Look up command in map
    for (size_t i = 0; i <serial_interface->command_count; i++) {
        if (serial_interface->command_maps[i].command == cmd) {
            uint8_t* data = NULL;
            size_t data_size = 0;
            ErrorCode result = mode_manager_execute_command(mode_manager, 
                                                         cmd,
                                                         return_size,
                                                         &data,
                                                         &data_size);
            LeaveCriticalSection(&serial_interface->mutex);
            return result;
        }
    }

    // If no handler found, just write the command
    ErrorCode result = serial_interface_write_data(serial_interface, command, command_size);
    LeaveCriticalSection(&serial_interface->mutex);
    return result;
}

ErrorCode serial_interface_register_command(SerialInterface*serial_interface, 
                                          IOCommand command,
                                          ModeExecuteFunc execute_func) {
    if (!serial_interface || !execute_func) {
        set_last_error(ERROR_INVALID_PARAM);
        return ERROR_INVALID_PARAM;
    }

    EnterCriticalSection(&serial_interface->mutex);

    // Grow array if needed
    size_t new_count =serial_interface->command_count + 1;
    CommandMap* new_maps = realloc(serial_interface->command_maps, new_count * sizeof(CommandMap));
    if (!new_maps) {
        LeaveCriticalSection(&serial_interface->mutex);
        return ERROR_OUT_OF_MEMORY;
    }

    // Add the new command
   serial_interface->command_maps = new_maps;
   serial_interface->command_maps[serial_interface->command_count].command = command;
   serial_interface->command_maps[serial_interface->command_count].execute_func = execute_func;
   serial_interface->command_count = new_count;

    LeaveCriticalSection(&serial_interface->mutex);
    return ERROR_NONE;
}



