#ifndef ERROR_CODES_H
#define ERROR_CODES_H

// Error codes enum
typedef enum {
    ERROR_NONE = 0,
    ERROR_INVALID_COMMAND = -1,
    ERROR_HANDSHAKE_FAILED = -2,
    ERROR_DEVICE_DISCONNECTED = -3,
    ERROR_TIME_OUT = -4,
    ERROR_WRITE_FAILED = -5,
    ERROR_READ_FAILED = -6,
    ERROR_BUFF_OVERFLOW = -7,
    ERROR_INVALID_MODE = -8,
    ERROR_PORT_NOT_FOUND = -9,
    ERROR_INVALID_TIMEOUT = -10,
    ERROR_FRAMING = -11,
    ERROR_PARITY = -12,
    ERROR_OVERRUN = -13,
    ERROR_INVALID_PARAM = -14,
    ERROR_MEMORY_ALLOCATION = -15,
    ERROR_DATA_INVALID = -16,
    ERROR_ACCESS_DENIED_ = -17,
    ERROR_INVALID_SETTINGS = -18,
    ERROR_BREAK_CONDITION = -19,
    ERROR_BUFFER_EMPTY = -20,
    ERROR_SERIAL_EXCEPTION = -21,
    ERROR_THREAD_CREATE = -22,
    ERROR_LIMIT_EXCEEDED = -23,
    ERROR_PROCESS_CREATE = -24,
    ERROR_PROCESS_TERMINATE = -25,
    ERROR_FILE_OPERATION = -26,
    ERROR_INVALID_VIDEO = -27
} ErrorCode;

// Error handling functions
const char* get_error_string(ErrorCode code);
void set_last_error(ErrorCode code);
ErrorCode get_last_error(void);

#endif // ERROR_CODES_H