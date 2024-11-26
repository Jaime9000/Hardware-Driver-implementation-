#include "service.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations of static functions
static ErrorCode create_named_pipe(ServiceContext* service);
static ErrorCode handle_pipe_connection(ServiceContext* service);
static ErrorCode process_pipe_message(ServiceContext* service, 
                                    const PipeMessage* message);
static DWORD WINAPI stream_thread_proc(LPVOID param);
static ErrorCode send_pipe_response(ServiceContext* service, 
                                  MessageType type, 
                                  const void* data, 
                                  uint32_t length);

ServiceContext* service_create(Config* config) {
    if (!config) return NULL;

    ServiceContext* service = (ServiceContext*)malloc(sizeof(ServiceContext));
    if (!service) return NULL;

    // Initialize members
    memset(service, 0, sizeof(ServiceContext));
    service->config = config;
    service->stop_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    service->is_running = false;
    service->is_streaming = false;
    service->pipe = INVALID_HANDLE_VALUE;
    service->stream_thread = NULL;
    InitializeCriticalSection(&service->stream_lock);
    
    // Create serial interface
    service->serial_interface = serial_interface_create(config);
    if (!service->serial_interface) {
        DeleteCriticalSection(&service->stream_lock);
        CloseHandle(service->stop_event);
        free(service);
        return NULL;
    }

    return service;
}

void service_destroy(ServiceContext* service) {
    if (!service) return;

    // Stop service if running
    service_stop(service);

    // Stop streaming if active
    if (service->is_streaming) {
        service_stop_data_stream(service);
    }

    // Cleanup resources
    if (service->pipe != INVALID_HANDLE_VALUE) {
        CloseHandle(service->pipe);
    }
    if (service->stop_event) {
        CloseHandle(service->stop_event);
    }
    if (service->serial_interface) {
        serial_interface_destroy(service->serial_interface);
    }

    DeleteCriticalSection(&service->stream_lock);
    free(service);
}

ErrorCode service_run(ServiceContext* service) {
    if (!service) return ERROR_DEVICE_DISCONNECTED;

    ErrorCode result = create_named_pipe(service);
    if (result != ERROR_NONE) {
        return result;
    }

    service->is_running = true;

    while (service->is_running) {
        // Wait for client connection
        if (ConnectNamedPipe(service->pipe, NULL) || 
            GetLastError() == ERROR_PIPE_CONNECTED) {
            
            result = handle_pipe_connection(service);
            if (result != ERROR_NONE) {
                config_log(service->config, LOG_LEVEL_ERROR, 
                          "Pipe connection handling failed: %d", result);
            }
        }

        // Check stop event
        if (WaitForSingleObject(service->stop_event, 0) == WAIT_OBJECT_0) {
            break;
        }

        DisconnectNamedPipe(service->pipe);
    }

    return ERROR_NONE;
}

static ErrorCode create_named_pipe(ServiceContext* service) {
    service->pipe = CreateNamedPipe(
        PIPE_NAME,
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        MAX_INSTANCES,
        PIPE_BUFFER_SIZE,
        PIPE_BUFFER_SIZE,
        0,
        NULL
    );

    if (service->pipe == INVALID_HANDLE_VALUE) {
        return ERROR_WRITE_FAILED;
    }

    return ERROR_NONE;
}

static ErrorCode handle_pipe_connection(ServiceContext* service) {
    PipeMessage header;
    DWORD bytes_read;
    
    while (service->is_running) {
        // Read message header
        if (!ReadFile(service->pipe, &header, sizeof(PipeMessage), 
                     &bytes_read, NULL)) {
            return ERROR_READ_FAILED;
        }

        // Allocate buffer for message data if needed
        uint8_t* message_data = NULL;
        if (header.length > 0) {
            message_data = malloc(header.length);
            if (!message_data) {
                return ERROR_MEMORY;
            }
            
            if (!ReadFile(service->pipe, message_data, header.length,
                         &bytes_read, NULL)) {
                free(message_data);
                return ERROR_READ_FAILED;
            }
        }

        // Create complete message
        PipeMessage* complete_message = malloc(sizeof(PipeMessage) + header.length);
        if (!complete_message) {
            free(message_data);
            return ERROR_MEMORY;
        }
        
        memcpy(complete_message, &header, sizeof(PipeMessage));
        if (message_data) {
            memcpy(complete_message->data, message_data, header.length);
            free(message_data);
        }

        // Process message
        ErrorCode result = process_pipe_message(service, complete_message);
        free(complete_message);

        if (result != ERROR_NONE) {
            return result;
        }
    }

    return ERROR_NONE;
}

static ErrorCode process_pipe_message(ServiceContext* service, 
                                    const PipeMessage* message) {
    ErrorCode result = ERROR_NONE;

    switch (message->type) {
        case MESSAGE_TYPE_COMMAND:
            result = service_handle_command(service, (const char*)message->data);
            break;
            
        case MESSAGE_TYPE_DATA_STREAM:
            if (message->length >= 1) {
                bool start_stream = message->data[0] != 0;
                result = start_stream ? 
                    service_start_data_stream(service) :
                    service_stop_data_stream(service);
            }
            break;

        case MESSAGE_TYPE_MODE_CHANGE:
            result = service_handle_mode_change(service, 
                                             (const char*)message->data);
            break;

        case MESSAGE_TYPE_STATUS:
            result = service_get_device_status(service);
            break;

        case MESSAGE_TYPE_VERSION:
            result = service_get_version(service);
            break;

        case MESSAGE_TYPE_HANDSHAKE:
            if (message->length >= 1) {
                result = service_perform_handshake(service, 
                                                message->data[0] != 0);
            }
            break;

        default:
            result = ERROR_INVALID_COMMAND;
            break;
    }

    // Send response
    if (result != ERROR_NONE) {
        const char* error_msg = "Command failed";
        send_pipe_response(service, MESSAGE_TYPE_ERROR, 
                          error_msg, strlen(error_msg) + 1);
    }

    return result;
}

static DWORD WINAPI stream_thread_proc(LPVOID param) {
    ServiceContext* service = (ServiceContext*)param;
    unsigned char buffer[PIPE_BUFFER_SIZE];
    size_t length;
    
    while (service->is_streaming) {
        EnterCriticalSection(&service->stream_lock);
        
        ErrorCode result = serial_interface_read_data(
            service->serial_interface, 
            buffer, 
            &length, 
            PIPE_BUFFER_SIZE
        );

        if (result == ERROR_NONE && length > 0) {
            send_pipe_response(service, MESSAGE_TYPE_DATA_STREAM, 
                             buffer, length);
        }

        LeaveCriticalSection(&service->stream_lock);
        Sleep(1);  // Prevent tight loop
    }

    return 0;
}

ErrorCode service_start_data_stream(ServiceContext* service) {
    if (!service || service->is_streaming) {
        return ERROR_INVALID_COMMAND;
    }

    service->is_streaming = true;
    service->stream_thread = CreateThread(NULL, 0, stream_thread_proc, 
                                        service, 0, NULL);
    
    if (!service->stream_thread) {
        service->is_streaming = false;
        return ERROR_WRITE_FAILED;
    }

    return ERROR_NONE;
}

ErrorCode service_stop_data_stream(ServiceContext* service) {
    if (!service || !service->is_streaming) {
        return ERROR_NONE;
    }

    service->is_streaming = false;
    
    if (service->stream_thread) {
        WaitForSingleObject(service->stream_thread, INFINITE);
        CloseHandle(service->stream_thread);
        service->stream_thread = NULL;
    }

    return ERROR_NONE;
}

static ErrorCode send_pipe_response(ServiceContext* service, 
                                  MessageType type, 
                                  const void* data, 
                                  uint32_t length) {
    PipeMessage header = {
        .type = type,
        .length = length
    };

    DWORD bytes_written;
    
    // Write header
    if (!WriteFile(service->pipe, &header, sizeof(PipeMessage), 
                  &bytes_written, NULL)) {
        return ERROR_WRITE_FAILED;
    }

    // Write data if present
    if (data && length > 0) {
        if (!WriteFile(service->pipe, data, length, 
                      &bytes_written, NULL)) {
            return ERROR_WRITE_FAILED;
        }
    }

    return ERROR_NONE;
}

ErrorCode service_stop(ServiceContext* service) {
    if (!service || !service->is_running) {
        return ERROR_NONE;
    }

    service->is_running = false;
    SetEvent(service->stop_event);

    // Stop streaming if active
    if (service->is_streaming) {
        service_stop_data_stream(service);
    }

    return ERROR_NONE;
}