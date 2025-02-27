#include "src/core/error_codes.h"  // Full path to own header

// Static variable to store the last error code
static ErrorCode last_error = ERROR_NONE;

// Function to get a string representation of an error code
const char* get_error_string(ErrorCode code) {
    switch (code) {
        case ERROR_NONE: 
            return "No error";
        case ERROR_INVALID_COMMAND: 
            return "Invalid command";
        case ERROR_HANDSHAKE_FAILED: 
            return "Handshake failed";
        case ERROR_DEVICE_DISCONNECTED: 
            return "Device disconnected";
        case ERROR_TIME_OUT: 
            return "Timeout occurred";
        case ERROR_WRITE_FAILED: 
            return "Write operation failed";
        case ERROR_READ_FAILED: 
            return "Read operation failed";
        case ERROR_BUFF_OVERFLOW: 
            return "Buffer overflow";
        case ERROR_INVALID_MODE: 
            return "Invalid mode";
        case ERROR_PORT_NOT_FOUND: 
            return "Serial port not found";
        case ERROR_INVALID_TIMEOUT: 
            return "Invalid timeout value";
        case ERROR_FRAMING: 
            return "Framing error in serial communication";
        case ERROR_PARITY: 
            return "Parity error in serial communication";
        case ERROR_OVERRUN: 
            return "Buffer overrun in serial communication";
        case ERROR_INVALID_PARAM: 
            return "Invalid parameter provided";
        case ERROR_MEMORY_ALLOCATION: 
            return "Memory allocation failed";
        case ERROR_DATA_INVALID: 
            return "Invalid data received";
        case ERROR_ACCESS_DENIED_: 
            return "Access denied to serial port";
        case ERROR_INVALID_SETTINGS: 
            return "Invalid serial port settings";
        case ERROR_BREAK_CONDITION: 
            return "Break condition detected on serial port";
        case ERROR_BUFFER_EMPTY: 
            return "No data available in buffer";
        case ERROR_SERIAL_EXCEPTION:
            return "Serial communication exception";
        case ERROR_THREAD_CREATE:
            return "Failed to create thread";
        case ERROR_LIMIT_EXCEEDED:
            return "Limit exceeded";
        case ERROR_PROCESS_CREATE:
            return "Failed to create process";
        case ERROR_PROCESS_TERMINATE:
            return "Failed to terminate process";
        case ERROR_FILE_OPERATION:
            return "Failed to perform file operation";
        case ERROR_INVALID_VIDEO:
            return "Invalid video";
        case ERROR_CRYPTO:
            return "Crypto error";
        default: 
            return "Unknown error";
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