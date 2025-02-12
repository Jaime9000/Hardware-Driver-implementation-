#include "src/core/serial_interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cJSON.h>

// Helper function declarations
static ErrorCode configure_serial_port(SerialInterface* interface);
static ErrorCode set_timeouts(SerialInterface* interface);
static ErrorCode perform_handshake_attempt(SerialInterface* interface, const char* handshake_string);
static ErrorCode read_until_empty(SerialInterface* interface);
static ErrorCode validate_interface(SerialInterface* interface);

SerialInterface* serial_interface_create(Config* config) {
    if (!config) {
        log_error("Cannot create serial interface: NULL config");
        return NULL;
    }

    SerialInterface* interface = (SerialInterface*)malloc(sizeof(SerialInterface));
    if (!interface) {
        log_error("Failed to allocate memory for serial interface");
        return NULL;
    }

    // Initialize basic members
    interface->handle = INVALID_HANDLE_VALUE;
    InitializeCriticalSection(&interface->mutex);
    interface->config = config;
    interface->is_connected = false;
    interface->handshake_established = false;
    interface->baud_rate = FAST_BAUD_RATE;
    interface->logger = config->logger;

    // Initialize command maps
    interface->command_maps = NULL;
    interface->command_count = 0;

    // Get port name from config
    const char* port_name = config_get_port_name(config);
    if (!port_name && config->port_auto_detect) {
        ConfigError config_error = config_sense_ports(config);
        if (config_error != CONFIG_SUCCESS) {
            log_error("Failed to auto-detect serial port: %s", config_get_error_string(config_error));
            DeleteCriticalSection(&interface->mutex);
            free(interface);
            return NULL;
        }
        port_name = config_get_port_name(config);
    }

    if (!port_name) {
        log_error("No valid port name available");
        DeleteCriticalSection(&interface->mutex);
        free(interface);
        return NULL;
    }

    interface->port_name = _strdup(port_name);
    if (!interface->port_name) {
        log_error("Failed to allocate memory for port name");
        DeleteCriticalSection(&interface->mutex);
        free(interface);
        return NULL;
    }

    log_debug("Serial interface created successfully for port %s", interface->port_name);
    return interface;
}

void serial_interface_destroy(SerialInterface* interface) {
    if (interface) {
        log_debug("Destroying serial interface");
        EnterCriticalSection(&interface->mutex);
        
        if (interface->is_connected) {
            serial_interface_close(interface);
        }
        
        // Free command maps
        free(interface->command_maps);
        
        LeaveCriticalSection(&interface->mutex);
        DeleteCriticalSection(&interface->mutex);
        
        free(interface->port_name);
        free(interface);
    }
}

ErrorCode serial_interface_open(SerialInterface* interface) {
    if (!interface) {
        log_error("Cannot open null interface");
        return ERROR_INVALID_PARAM;
    }

    log_info("Opening serial port: %s", interface->port_name);
    EnterCriticalSection(&interface->mutex);

    if (interface->is_connected) {
        log_warning("Port already open, closing first");
        serial_interface_close(interface);
    }

    // Create the file handle with proper flags for serial communication
    interface->handle = CreateFile(interface->port_name,
                                 GENERIC_READ | GENERIC_WRITE,
                                 0,  // No sharing
                                 NULL,
                                 OPEN_EXISTING,
                                 FILE_ATTRIBUTE_NORMAL,
                                 NULL);

    if (interface->handle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        log_error("Failed to open serial port. System error: %lu", error);
        LeaveCriticalSection(&interface->mutex);
        return ERROR_DEVICE_DISCONNECTED;
    }

    // Configure the port
    ErrorCode error = configure_serial_port(interface);
    if (error != ERROR_NONE) {
        log_error("Failed to configure serial port");
        CloseHandle(interface->handle);
        interface->handle = INVALID_HANDLE_VALUE;
        LeaveCriticalSection(&interface->mutex);
        return error;
    }

    // Set the timeouts
    error = set_timeouts(interface);
    if (error != ERROR_NONE) {
        log_error("Failed to set port timeouts");
        CloseHandle(interface->handle);
        interface->handle = INVALID_HANDLE_VALUE;
        LeaveCriticalSection(&interface->mutex);
        return error;
    }

    interface->is_connected = true;
    interface->handshake_established = false;
    log_info("Serial port opened successfully");
    
    LeaveCriticalSection(&interface->mutex);
    return ERROR_NONE;
}

ErrorCode serial_interface_close(SerialInterface* interface) {
    ErrorCode result = validate_interface(interface);
    if (result != ERROR_NONE) {
        return result;
    }

    log_info("Closing serial port");
    EnterCriticalSection(&interface->mutex);

    // Ensure all data is written before closing
    if (interface->is_connected) {
        FlushFileBuffers(interface->handle);
        CloseHandle(interface->handle);
        interface->handle = INVALID_HANDLE_VALUE;
        interface->is_connected = false;
        interface->handshake_established = false;
    }

    LeaveCriticalSection(&interface->mutex);
    return ERROR_NONE;
}

static ErrorCode validate_interface(SerialInterface* interface) {
    if (!interface) {
        log_error("Null interface pointer");
        return ERROR_INVALID_PARAM;
    }

    // Check if we can actually communicate with the serial port
    DCB dcb = {0};
    if (!GetCommState(interface->handle, &dcb)) {
        log_error("Serial communication not available");
        return ERROR_SERIAL_EXCEPTION;  // Use new error code here
    }

    if (!interface->is_connected) {
        log_error("Device not connected");
        return ERROR_DEVICE_DISCONNECTED;
    }

    if (interface->handle == INVALID_HANDLE_VALUE) {
        log_error("Invalid handle value");
        return ERROR_DEVICE_DISCONNECTED;
    }

    return ERROR_NONE;
}

static ErrorCode configure_serial_port(SerialInterface* interface) {
    DCB dcb = {0};
    dcb.DCBlength = sizeof(DCB);

    if (!GetCommState(interface->handle, &dcb)) {
        DWORD error = GetLastError();
        log_error("Failed to get comm state. System error: %lu", error);
        return ERROR_DEVICE_DISCONNECTED;
    }

    // Configure serial parameters
    dcb.BaudRate = interface->baud_rate;
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

    if (!SetCommState(interface->handle, &dcb)) {
        DWORD error = GetLastError();
        log_error("Failed to set comm state. System error: %lu", error);
        return ERROR_DEVICE_DISCONNECTED;
    }

    return ERROR_NONE;
}

ErrorCode serial_interface_write_data(SerialInterface* interface, const unsigned char* data, size_t length) {
    ErrorCode result = validate_interface(interface);
    if (result != ERROR_NONE) {
        return result;
    }

    if (!data || length == 0) {
        log_error("Invalid write parameters");
        return ERROR_INVALID_PARAM;
    }

    log_debug("Writing %zu bytes to serial port", length);
    EnterCriticalSection(&interface->mutex);

    DWORD bytes_written;
    BOOL success = WriteFile(interface->handle, data, (DWORD)length, &bytes_written, NULL);
    
    if (!success || bytes_written != length) {
        DWORD error = GetLastError();
        log_error("Write failed. System error: %lu", error);
        LeaveCriticalSection(&interface->mutex);
        return ERROR_WRITE_FAILED;
    }

    // Ensure data is actually sent
    if (!FlushFileBuffers(interface->handle)) {
        DWORD error = GetLastError();
        log_error("Failed to flush buffers. System error: %lu", error);
        LeaveCriticalSection(&interface->mutex);
        return ERROR_WRITE_FAILED;
    }

    LeaveCriticalSection(&interface->mutex);
    log_debug("Successfully wrote %lu bytes", bytes_written);
    return ERROR_NONE;
}

ErrorCode serial_interface_read_data(SerialInterface* interface, unsigned char* buffer, size_t* bytes_read, size_t max_length) {
    ErrorCode result = validate_interface(interface);
    if (result != ERROR_NONE) {
        return result;
    }

    if (!buffer || !bytes_read || max_length == 0) {
        log_error("Invalid read parameters");
        return ERROR_INVALID_PARAM;
    }

    log_debug("Attempting to read up to %zu bytes", max_length);
    EnterCriticalSection(&interface->mutex);

    DWORD bytes_read_local;
    BOOL success = ReadFile(interface->handle, buffer, (DWORD)max_length, &bytes_read_local, NULL);
    
    if (!success) {
        DWORD error = GetLastError();
        log_error("Read failed. System error: %lu", error);
        *bytes_read = 0;
        LeaveCriticalSection(&interface->mutex);
        return ERROR_READ_FAILED;
    }

    *bytes_read = bytes_read_local;
    LeaveCriticalSection(&interface->mutex);
    log_debug("Successfully read %lu bytes", bytes_read_local);
    return ERROR_NONE;
}

ErrorCode serial_interface_flush(SerialInterface* interface) {
    ErrorCode result = validate_interface(interface);
    if (result != ERROR_NONE) {
        return result;
    }

    log_debug("Flushing serial port buffers");
    EnterCriticalSection(&interface->mutex);

    if (!FlushFileBuffers(interface->handle)) {
        DWORD error = GetLastError();
        log_error("Failed to flush buffers. System error: %lu", error);
        LeaveCriticalSection(&interface->mutex);
        return ERROR_WRITE_FAILED;
    }

    LeaveCriticalSection(&interface->mutex);
    return ERROR_NONE;
}

// USB Control Functions
ErrorCode serial_interface_usb_control_on(SerialInterface* interface) {
    ErrorCode result = validate_interface(interface);
    if (result != ERROR_NONE) {
        return result;
    }

    log_debug("Setting RTS control on");
    EnterCriticalSection(&interface->mutex);

    if (!EscapeCommFunction(interface->handle, SETRTS)) {
        DWORD error = GetLastError();
        log_error("Failed to set RTS. System error: %lu", error);
        LeaveCriticalSection(&interface->mutex);
        return ERROR_WRITE_FAILED;
    }

    if (!FlushFileBuffers(interface->handle)) {
        DWORD error = GetLastError();
        log_error("Failed to flush buffers. System error: %lu", error);
        LeaveCriticalSection(&interface->mutex);
        return ERROR_WRITE_FAILED;
    }

    LeaveCriticalSection(&interface->mutex);
    return ERROR_NONE;
}

ErrorCode serial_interface_usb_control_off(SerialInterface* interface) {
    ErrorCode result = validate_interface(interface);
    if (result != ERROR_NONE) {
        return result;
    }

    log_debug("Setting RTS control off");
    EnterCriticalSection(&interface->mutex);

    if (!EscapeCommFunction(interface->handle, CLRRTS)) {
        DWORD error = GetLastError();
        log_error("Failed to clear RTS. System error: %lu", error);
        LeaveCriticalSection(&interface->mutex);
        return ERROR_WRITE_FAILED;
    }

    if (!FlushFileBuffers(interface->handle)) {
        DWORD error = GetLastError();
        log_error("Failed to flush buffers. System error: %lu", error);
        LeaveCriticalSection(&interface->mutex);
        return ERROR_WRITE_FAILED;
    }

    LeaveCriticalSection(&interface->mutex);
    return ERROR_NONE;
}

ErrorCode serial_interface_usb_data_on(SerialInterface* interface) {
    ErrorCode result = validate_interface(interface);
    if (result != ERROR_NONE) {
        return result;
    }

    log_debug("Setting DTR control on");
    EnterCriticalSection(&interface->mutex);

    if (!EscapeCommFunction(interface->handle, SETDTR)) {
        DWORD error = GetLastError();
        log_error("Failed to set DTR. System error: %lu", error);
        LeaveCriticalSection(&interface->mutex);
        return ERROR_WRITE_FAILED;
    }

    if (!FlushFileBuffers(interface->handle)) {
        DWORD error = GetLastError();
        log_error("Failed to flush buffers. System error: %lu", error);
        LeaveCriticalSection(&interface->mutex);
        return ERROR_WRITE_FAILED;
    }

    LeaveCriticalSection(&interface->mutex);
    return ERROR_NONE;
}

ErrorCode serial_interface_usb_data_off(SerialInterface* interface) {
    ErrorCode result = validate_interface(interface);
    if (result != ERROR_NONE) {
        return result;
    }

    log_debug("Setting DTR control off");
    EnterCriticalSection(&interface->mutex);

    if (!EscapeCommFunction(interface->handle, CLRDTR)) {
        DWORD error = GetLastError();
        log_error("Failed to clear DTR. System error: %lu", error);
        LeaveCriticalSection(&interface->mutex);
        return ERROR_WRITE_FAILED;
    }

    if (!FlushFileBuffers(interface->handle)) {
        DWORD error = GetLastError();
        log_error("Failed to flush buffers. System error: %lu", error);
        LeaveCriticalSection(&interface->mutex);
        return ERROR_WRITE_FAILED;
    }

    LeaveCriticalSection(&interface->mutex);
    return ERROR_NONE;
}

ErrorCode serial_interface_reset_hardware(SerialInterface* interface, bool is_60hz) {
    ErrorCode result = validate_interface(interface);
    if (result != ERROR_NONE) {
        return result;
    }

    log_info("Resetting hardware for %dHz operation", is_60hz ? 60 : 50);
    const char* handshake_string = is_60hz ? HANDSHAKE_STRING_60_HZ : HANDSHAKE_STRING_50_HZ;

    // Update frequency config in the system
    result = serial_interface_set_current_freq_config(interface, is_60hz ? 60 : 50);
    if (result != ERROR_NONE) {
        log_error("Failed to update frequency configuration");
        return result;
    }

    return serial_interface_perform_handshake(interface, handshake_string);
}

ErrorCode serial_interface_perform_handshake(SerialInterface* interface, const char* handshake_string) {
    ErrorCode result = validate_interface(interface);
    if (result != ERROR_NONE) {
        return result;
    }

    log_info("Performing handshake with string: %s", handshake_string);
    EnterCriticalSection(&interface->mutex);

    // Reset handshake status
    interface->handshake_established = false;

    // Try handshake multiple times
    for (int attempt = 0; attempt < MAX_HANDSHAKE_ATTEMPTS; attempt++) {
        log_debug("Handshake attempt %d of %d", attempt + 1, MAX_HANDSHAKE_ATTEMPTS);
        
        result = perform_handshake_attempt(interface, handshake_string);
        if (result == ERROR_NONE) {
            interface->handshake_established = true;
            log_info("Handshake successful");
            LeaveCriticalSection(&interface->mutex);
            return ERROR_NONE;
        }
        
        // Small delay between attempts
        Sleep(100);
    }

    log_error("Handshake failed after %d attempts", MAX_HANDSHAKE_ATTEMPTS);
    LeaveCriticalSection(&interface->mutex);
    return ERROR_HANDSHAKE_FAILED;
}

static ErrorCode perform_handshake_attempt(SerialInterface* interface, const char* handshake_string) {
    // Turn off USB control lines
    serial_interface_usb_control_off(interface);
    serial_interface_usb_data_off(interface);
    
    // Clear any pending data
    serial_interface_reset_buffers(interface);
    read_until_empty(interface);

    // Turn on USB control lines
    serial_interface_usb_control_on(interface);
    serial_interface_usb_data_on(interface);
    
    // Send handshake string
    ErrorCode result = serial_interface_write_data(interface, 
        (const unsigned char*)handshake_string, 
        strlen(handshake_string));
    
    if (result != ERROR_NONE) {
        return result;
    }

    // Read response
    unsigned char buffer[MAX_VERSION_STRING_LENGTH];
    size_t bytes_read;
    result = serial_interface_read_data(interface, buffer, &bytes_read, sizeof(buffer) - 1);
    
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

ErrorCode serial_interface_control_statuses(SerialInterface* interface, char* status_string, bool as_json) {
    ErrorCode result = validate_interface(interface);
    if (result != ERROR_NONE) {
        return result;
    }

    DWORD status;
    if (!GetCommModemStatus(interface->handle, &status)) {
        log_error("Failed to get modem status");
        return ERROR_READ_FAILED;
    }

    bool rts = serial_interface_get_rts_status(interface);
    bool dtr = serial_interface_get_dtr_status(interface);
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

ErrorCode serial_interface_get_current_freq_config(SerialInterface* interface, int* frequency) {
    if (!interface || !frequency) {
        return ERROR_INVALID_PARAM;
    }

    FILE* file = fopen(interface->config->freq_config_path, "r");
    if (!file) {
        log_error("Failed to open frequency config file");
        return ERROR_FILE_ACCESS;
    }

    char freq_str[8];
    if (fgets(freq_str, sizeof(freq_str), file) == NULL) {
        fclose(file);
        log_error("Failed to read frequency config");
        return ERROR_FILE_ACCESS;
    }

    fclose(file);
    *frequency = atoi(freq_str);
    
    if (*frequency != 50 && *frequency != 60) {
        log_error("Invalid frequency value: %d", *frequency);
        return ERROR_INVALID_CONFIG;
    }

    return ERROR_NONE;
}

ErrorCode serial_interface_set_current_freq_config(SerialInterface* interface, int frequency) {
    if (!interface) {
        return ERROR_INVALID_PARAM;
    }

    if (frequency != 50 && frequency != 60) {
        log_error("Invalid frequency value: %d", frequency);
        return ERROR_INVALID_PARAM;
    }

    FILE* file = fopen(interface->config->freq_config_path, "w");
    if (!file) {
        log_error("Failed to open frequency config file for writing");
        return ERROR_FILE_ACCESS;
    }

    if (fprintf(file, "%d", frequency) < 0) {
        fclose(file);
        log_error("Failed to write frequency config");
        return ERROR_FILE_ACCESS;
    }

    fclose(file);
    return ERROR_NONE;
}

// Status getter functions
bool serial_interface_get_rts_status(SerialInterface* interface) {
    if (!interface || !interface->is_connected) {
        return false;
    }

    DWORD modemStatus;
    if (GetCommModemStatus(interface->handle, &modemStatus)) {
        return (modemStatus & MS_RTS_ON) != 0;
    }
    return false;
}

bool serial_interface_get_dtr_status(SerialInterface* interface) {
    if (!interface || !interface->is_connected) {
        return false;
    }

    DWORD modemStatus;
    if (GetCommModemStatus(interface->handle, &modemStatus)) {
        return (modemStatus & MS_DTR_ON) != 0;
    }
    return false;
}

bool serial_interface_get_cts_status(SerialInterface* interface) {
    if (!interface || !interface->is_connected) {
        return false;
    }

    DWORD modemStatus;
    if (GetCommModemStatus(interface->handle, &modemStatus)) {
        return (modemStatus & MS_CTS_ON) != 0;
    }
    return false;
}

bool serial_interface_get_dsr_status(SerialInterface* interface) {
    if (!interface || !interface->is_connected) {
        return false;
    }

    DWORD modemStatus;
    if (GetCommModemStatus(interface->handle, &modemStatus)) {
        return (modemStatus & MS_DSR_ON) != 0;
    }
    return false;
}

bool serial_interface_get_cd_status(SerialInterface* interface) {
    if (!interface || !interface->is_connected) {
        return false;
    }

    DWORD modemStatus;
    if (GetCommModemStatus(interface->handle, &modemStatus)) {
        return (modemStatus & MS_RLSD_ON) != 0;
    }
    return false;
}

bool serial_interface_get_ri_status(SerialInterface* interface) {
    if (!interface || !interface->is_connected) {
        return false;
    }

    DWORD modemStatus;
    if (GetCommModemStatus(interface->handle, &modemStatus)) {
        return (modemStatus & MS_RING_ON) != 0;
    }
    return false;
}

bool serial_interface_is_handshake_established(SerialInterface* interface) {
    return interface && interface->handshake_established;
}

// Buffer management
ErrorCode serial_interface_reset_buffers(SerialInterface* interface) {
    ErrorCode result = validate_interface(interface);
    if (result != ERROR_NONE) {
        return result;
    }

    log_debug("Resetting serial port buffers");
    EnterCriticalSection(&interface->mutex);

    if (!PurgeComm(interface->handle, 
                   PURGE_RXCLEAR | PURGE_TXCLEAR | 
                   PURGE_RXABORT | PURGE_TXABORT)) {
        DWORD error = GetLastError();
        log_error("Failed to purge comm buffers. System error: %lu", error);
        LeaveCriticalSection(&interface->mutex);
        return ERROR_WRITE_FAILED;
    }

    LeaveCriticalSection(&interface->mutex);
    return ERROR_NONE;
}

// Helper function to read until no more data is available
static ErrorCode read_until_empty(SerialInterface* interface) {
    unsigned char buffer[256];
    size_t bytes_read;
    
    while (true) {
        ErrorCode result = serial_interface_read_data(interface, buffer, &bytes_read, sizeof(buffer));
        if (result != ERROR_NONE || bytes_read == 0) {
            break;
        }
    }
    
    return ERROR_NONE;
}

// Timeout configuration
static ErrorCode set_timeouts(SerialInterface* interface) {
    COMMTIMEOUTS timeouts = {0};
    
    // Configure read timeouts
    timeouts.ReadIntervalTimeout = MAXDWORD;  // Return immediately if no data
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = DEFAULT_TIMEOUT;
    
    // Configure write timeouts
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = DEFAULT_TIMEOUT;

    if (!SetCommTimeouts(interface->handle, &timeouts)) {
        DWORD error = GetLastError();
        log_error("Failed to set comm timeouts. System error: %lu", error);
        return ERROR_DEVICE_DISCONNECTED;
    }

    return ERROR_NONE;
}

// EMG Version and Equipment byte functions
ErrorCode serial_interface_get_emg_version(SerialInterface* interface, char* version_string, size_t max_length) {
    ErrorCode result = validate_interface(interface);
    if (result != ERROR_NONE) {
        return result;
    }

    if (!version_string || max_length == 0) {
        return ERROR_INVALID_PARAM;
    }

    // Send version request command
    const char* cmd = "get-emg-version";
    result = serial_interface_write_data(interface, (const unsigned char*)cmd, strlen(cmd));
    if (result != ERROR_NONE) {
        return result;
    }

    // Read response
    unsigned char buffer[MAX_VERSION_STRING_LENGTH];
    size_t bytes_read;
    result = serial_interface_read_data(interface, buffer, &bytes_read, sizeof(buffer) - 1);
    
    if (result != ERROR_NONE) {
        return result;
    }

    // Ensure null termination and copy to output
    buffer[bytes_read] = '\0';
    strncpy(version_string, (char*)buffer, max_length - 1);
    version_string[max_length - 1] = '\0';

    return ERROR_NONE;
}

ErrorCode serial_interface_get_equipment_byte(SerialInterface* interface, unsigned char* equipment_byte) {
    ErrorCode result = validate_interface(interface);
    if (result != ERROR_NONE) {
        return result;
    }

    if (!equipment_byte) {
        return ERROR_INVALID_PARAM;
    }

    // Send equipment byte request command
    const char* cmd = "get-equipment-byte";
    result = serial_interface_write_data(interface, (const unsigned char*)cmd, strlen(cmd));
    if (result != ERROR_NONE) {
        return result;
    }

    // Read response
    size_t bytes_read;
    result = serial_interface_read_data(interface, equipment_byte, &bytes_read, 1);
    
    if (result != ERROR_NONE || bytes_read != 1) {
        return ERROR_READ_FAILED;
    }

    return ERROR_NONE;
}

ErrorCode serial_interface_check_connection(SerialInterface* interface) {
    return validate_interface(interface);
}

ErrorCode serial_interface_execute_command(SerialInterface* interface, 
                                         const uint8_t* command,
                                         size_t command_size,
                                         ModeManager* mode_manager) {
    if (!interface || !command || !mode_manager) {
        set_last_error(ERROR_INVALID_PARAM);
        return ERROR_INVALID_PARAM;
    }

    // Parse command and return size
    size_t return_size = 0;
    IOCommand cmd;
    if (parse_command_and_size(command, command_size, &cmd, &return_size) != ERROR_NONE) {
        return ERROR_INVALID_COMMAND;
    }

    EnterCriticalSection(&interface->mutex);

    // Look up command in map
    for (size_t i = 0; i < interface->command_count; i++) {
        if (interface->command_maps[i].command == cmd) {
            uint8_t* data = NULL;
            size_t data_size = 0;
            ErrorCode result = mode_manager_execute_command(mode_manager, 
                                                         cmd,
                                                         return_size,
                                                         &data,
                                                         &data_size);
            LeaveCriticalSection(&interface->mutex);
            return result;
        }
    }

    // If no handler found, just write the command
    ErrorCode result = serial_interface_write_data(interface, command, command_size);
    LeaveCriticalSection(&interface->mutex);
    return result;
}

ErrorCode serial_interface_register_command(SerialInterface* interface, 
                                          IOCommand command,
                                          ModeExecuteFunc execute_func) {
    if (!interface || !execute_func) {
        set_last_error(ERROR_INVALID_PARAM);
        return ERROR_INVALID_PARAM;
    }

    EnterCriticalSection(&interface->mutex);

    // Grow array if needed
    size_t new_count = interface->command_count + 1;
    CommandMap* new_maps = realloc(interface->command_maps, new_count * sizeof(CommandMap));
    if (!new_maps) {
        LeaveCriticalSection(&interface->mutex);
        return ERROR_OUT_OF_MEMORY;
    }

    // Add the new command
    interface->command_maps = new_maps;
    interface->command_maps[interface->command_count].command = command;
    interface->command_maps[interface->command_count].execute_func = execute_func;
    interface->command_count = new_count;

    LeaveCriticalSection(&interface->mutex);
    return ERROR_NONE;
}



