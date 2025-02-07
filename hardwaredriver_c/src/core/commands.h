#ifndef MYOTRONICS_COMMANDS_H
#define MYOTRONICS_COMMANDS_H

#include <stdio.h>
#include <stdbool.h>
#include "src/core/error_codes.h"

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

// EMG Configuration options
#define EMG_CONFIG_RAW 'r'
#define EMG_CONFIG_PROCESSED 'p'

// Mode command strings
// Mode 0 variants
#define CMD_MODE_0_CONF "mode-0"
#define CMD_MODE_0_RAW "mode-0-raw"
#define CMD_MODE_0_ALIGN "mode-0-align"

// Mode 42 variants
#define CMD_MODE_42_RAW "mode-42-raw"
#define CMD_MODE_42_RAW_Q "mode-42-raw-q"
#define CMD_MODE_42_RAW_S "mode-42-raw-s"
#define CMD_MODE_42_RAW_U "mode-42-raw-u"
#define CMD_MODE_42_RAW_W "mode-42-raw-w"
#define CMD_MODE_42_RAW_T "mode-42-raw-t"
#define CMD_MODE_42_RAW_V "mode-42-raw-v"
#define CMD_MODE_42_RAW_P "mode-42-raw-p"
#define CMD_MODE_42_RAW_R "mode-42-raw-r"

// Mode 43 variants
#define CMD_MODE_43_RAW "mode-43-raw"
#define CMD_MODE_43_RAW_Q "mode-43-raw-q"
#define CMD_MODE_43_RAW_S "mode-43-raw-s"
#define CMD_MODE_43_RAW_U "mode-43-raw-u"
#define CMD_MODE_43_RAW_W "mode-43-raw-w"
#define CMD_MODE_43_RAW_T "mode-43-raw-t"
#define CMD_MODE_43_RAW_V "mode-43-raw-v"
#define CMD_MODE_43_RAW_P "mode-43-raw-p"
#define CMD_MODE_43_RAW_R "mode-43-raw-r"
#define CMD_MODE_43_EMG "mode-43-emg"

// Mode 44 variants
#define CMD_MODE_44_RAW "mode-44-raw"
#define CMD_MODE_44_RAW_NO_IMAGE "mode-44-raw-no-image"
#define CMD_MODE_44_SWEEP "mode-44-sweep"

// Other mode commands
#define CMD_MODE_51_RAW "mode-51-raw"
#define CMD_MODE_52_RAW "mode-52-raw"
#define CMD_MODE_53_RAW "mode-53-raw"
#define CMD_MODE_56_RAW "mode-56-raw"
#define CMD_MODE_57_RAW "mode-57-raw"
#define CMD_MODE_57_RAW_NO_IMAGE "mode-57-raw-no-image"

// Special commands
#define CMD_MODE_SWEEP "mode-sweep"
#define CMD_EMG_VERSION "emg-version"
#define CMD_CHECK_CONNECTION "check-connection"
#define CMD_GET_EQUIPMENT_BYTE "get-equipment-byte"
#define CMD_GET_EMG_LEAD_STATUS "get-emg-lead-status"

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
// Buffer sizes and timeouts
#define MAX_COMMAND_LENGTH 32
#define DEFAULT_BUFFER_SIZE 1600
#define MAX_BUFFER_SIZE 32000
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

// Function declarations
const char* get_mode_command_string(ModeNumber mode);
ModeConfig* get_mode_config(const char* command_string);
bool validate_mode_command(const char* command);
bool validate_control_command(const char* command);
ErrorCode parse_command(const char* command, IOCommand* io_cmd, ModeConfig* mode_cfg);

#ifdef __cplusplus
}
#endif

#endif // MYOTRONICS_COMMANDS_H