#include "error_codes.h"

// Static variable to store the last error code
static ErrorCode last_error = ERROR_NONE;

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