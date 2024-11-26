#ifndef MYOTRONICS_COMMANDS_H
#define MYOTRONICS_COMMANDS_H

#include <stdbool.h>

// Main IO Commands
typedef enum {
    RTS_ON = '1',
    RTS_OFF = '2',
    DTR_ON = '3',
    DTR_OFF = '4',
    RESET_HARDWARE_60 = '15',  // flushes data and re-executes handshake at 60 hz
    RESET_HARDWARE_50 = '17',  // flushes data and re-executes handshake at 50 hz
    DEVICE_STATUSES = '16',
} IOCommand;

// Mode definitions
typedef enum {
    MODE_0 = 0,
    MODE_42 = 42,
    MODE_43 = 43,
    MODE_44 = 44,
    MODE_51 = 51,
    MODE_52 = 52,
    MODE_53 = 53,
    MODE_56 = 56,
    MODE_57 = 57,
    MODE_118 = 118,  // EMG Version
} ModeNumber;

// Mode command strings
#define CMD_MODE_0_CONF "mode-0"
#define CMD_MODE_0_RAW "mode-0-raw"
#define CMD_MODE_0_ALIGN "mode-0-align"
#define CMD_MODE_42_RAW "mode-42-raw"
#define CMD_MODE_43_RAW "mode-43-raw"
#define CMD_MODE_44_RAW "mode-44-raw"
#define CMD_MODE_44_RAW_NO_IMAGE "mode-44-raw-no-image"
#define CMD_MODE_44_SWEEP "mode-44-sweep"
#define CMD_MODE_51_RAW "mode-51-raw"
#define CMD_MODE_52_RAW "mode-52-raw"
#define CMD_MODE_53_RAW "mode-53-raw"
#define CMD_MODE_56_RAW "mode-56-raw"
#define CMD_MODE_57_RAW "mode-57-raw"
#define CMD_MODE_57_RAW_NO_IMAGE "mode-57-raw-no-image"
#define CMD_MODE_SWEEP "mode-sweep"
#define CMD_EMG_VERSION "emg-version"
#define CMD_CHECK_CONNECTION "check-connection"

// EMG Configuration options
#define EMG_CONFIG_RAW 'r'
#define EMG_CONFIG_PROCESSED 'p'

// Mode configuration structure
typedef struct {
    ModeNumber mode_number;
    char emg_config;
    bool requires_handshake;
    bool supports_disconnected;
    int default_byte_count;
} ModeConfig;

// Helper functions
const char* get_mode_command_string(ModeNumber mode);
ModeConfig* get_mode_config(const char* command_string);
bool is_valid_command(const char* command);

// Handshake strings
#define HANDSHAKE_STRING_50_HZ "K7-MYO5"
#define HANDSHAKE_STRING_60_HZ "K7-MYO6"
#define HANDSHAKE_VERSION_STRING "K7-MYO Ver"

// Device response codes
#define DEVICE_RESPONSE_SUCCESS 0x00
#define DEVICE_RESPONSE_ERROR 0x01
#define DEVICE_RESPONSE_TIMEOUT 0x02
#define DEVICE_RESPONSE_DISCONNECTED 0x03

// Default timeout values (in milliseconds)
#define DEFAULT_READ_TIMEOUT 500
#define DEFAULT_WRITE_TIMEOUT 500
#define DEFAULT_HANDSHAKE_TIMEOUT 1000

// Buffer sizes
#define MAX_COMMAND_LENGTH 32
#define DEFAULT_BUFFER_SIZE 1600
#define MAX_BUFFER_SIZE 32000

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

// Mode status flags
typedef enum {
    MODE_STATUS_IDLE = 0,
    MODE_STATUS_RUNNING = 1,
    MODE_STATUS_ERROR = 2,
    MODE_STATUS_DISCONNECTED = 3,
} ModeStatus;

// Implementation helper macros
#define IS_MODE_COMMAND(cmd) ((cmd)[0] == 'm')
#define IS_EMG_COMMAND(cmd) ((cmd)[0] == 'e')
#define IS_CONTROL_COMMAND(cmd) ((cmd)[0] >= '1' && (cmd)[0] <= '9')

#ifdef __cplusplus
extern "C" {
#endif

// Command validation function declarations
bool validate_mode_command(const char* command);
bool validate_control_command(const char* command);
ErrorCode parse_command(const char* command, IOCommand* io_cmd, ModeConfig* mode_cfg);

// Error handling
const char* get_error_string(ErrorCode code);
void set_last_error(ErrorCode code);
ErrorCode get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif // MYOTRONICS_COMMANDS_H