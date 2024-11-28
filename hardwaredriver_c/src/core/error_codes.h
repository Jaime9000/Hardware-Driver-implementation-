#ifndef ERROR_CODES_H
#define ERROR_CODES_H

// Error codes
typedef enum {
    ERROR_NONE = 0,
    ERROR_INVALID_COMMAND = -1,
    ERROR_HANDSHAKE_FAILED = -2,
    ERROR_DEVICE_DISCONNECTED = -3,
    ERROR_TIMEOUT = -4,
    ERROR_WRITE_FAILED = -5,
    ERROR_READ_FAILED = -6,
    ERROR_BUFFER_OVERFLOW = -7,
    ERROR_INVALID_MODE = -8,
} ErrorCode;

// Error handling functions
const char* get_error_string(ErrorCode code);
void set_last_error(ErrorCode code);
ErrorCode get_last_error(void);

#endif // ERROR_CODES_H