#ifndef MYOTRONICS_SERVICE_H
#define MYOTRONICS_SERVICE_H

#include <stdio.h>
#include <windows.h>
#include <stdint.h>
#include "config.h"
#include "serial_interface.h"
#include "commands.h"
#include "error_codes.h"

#define PIPE_NAME "\\\\.\\pipe\\MyotronicsDriver"
#define PIPE_BUFFER_SIZE 4096
#define MAX_INSTANCES 1
#define MAX_COMMAND_LENGTH 256

// Message types for pipe communication
typedef enum {
    MESSAGE_TYPE_COMMAND,      // Command to device
    MESSAGE_TYPE_DATA_STREAM,  // Streaming data
    MESSAGE_TYPE_STATUS,       // Device status
    MESSAGE_TYPE_VERSION,      // Version info
    MESSAGE_TYPE_ERROR,        // Error message
    MESSAGE_TYPE_MODE_CHANGE,  // Mode change request
    MESSAGE_TYPE_HANDSHAKE     // Device handshake
} MessageType;

// Message structure for pipe communication
typedef struct {
    MessageType type;
    uint32_t length;
    uint8_t data[];  // Flexible array member
} PipeMessage;

// Service context structure
typedef struct {
    Config* config;
    SerialInterface* serial_interface;
    HANDLE pipe;
    HANDLE stop_event;
    HANDLE stream_thread;
    char previous_command[MAX_COMMAND_LENGTH];
    BYTE device_bit;
    char hardware_identifier[64];
    bool is_running;
    bool is_streaming;
    CRITICAL_SECTION stream_lock;
} ServiceContext;

// Constructor/Destructor
ServiceContext* service_create(Config* config);
void service_destroy(ServiceContext* service);

// Service control
ErrorCode service_run(ServiceContext* service);
ErrorCode service_stop(ServiceContext* service);

// Stream control
ErrorCode service_start_data_stream(ServiceContext* service);
ErrorCode service_stop_data_stream(ServiceContext* service);

// Command handling
ErrorCode service_handle_command(ServiceContext* service, const char* command);
ErrorCode service_handle_mode_change(ServiceContext* service, const char* mode);
ErrorCode service_get_device_status(ServiceContext* service);
ErrorCode service_get_version(ServiceContext* service);
ErrorCode service_perform_handshake(ServiceContext* service, bool is_60hz);

#endif // MYOTRONICS_SERVICE_H