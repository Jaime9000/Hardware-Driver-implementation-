#ifndef MYOTRONICS_SERVICE_H
#define MYOTRONICS_SERVICE_H

#include <windows.h>
#include <http.h>

// Project includes with correct paths
#include "src/core/main/config.h"
#include "src/core/serial_interface.h"         // Corrected path
#include "src/core/error_codes.h"              // Corrected path
#include "src/core/mode_functions/mode_manager.h"
#include "src/gui/utils/process_manager.h"     // Corrected path - it's in gui/utils

#define DEFAULT_PORT 8080 // Changed from 8000 to 8080
#define MAX_COMMAND_LENGTH 256

typedef struct {
    Config* config;
    SerialInterface* serial_interface;
    ModeManager* mode_manager;
    ProcessManager* process_manager;
    HANDLE stop_event;
    HANDLE http_thread;
    HTTP_SERVER_SESSION_ID session_id;
    HTTP_URL_GROUP_ID url_group_id;
    HANDLE request_queue;
    char previous_command[MAX_COMMAND_LENGTH];
    BYTE device_bit;
    char hardware_identifier[64];
    bool is_running;
} ServiceContext;

// Constructor/Destructor
ServiceContext* service_create(Config* config);
void service_destroy(ServiceContext* service);

// Service control
ErrorCode service_run(ServiceContext* service);
ErrorCode service_stop(ServiceContext* service);

// HTTP request handling
ErrorCode service_handle_request(ServiceContext* service, HTTP_REQUEST* request);
ErrorCode service_send_response(ServiceContext* service, HTTP_REQUEST_ID requestId, 
                              USHORT status_code, const char* response_data);

// Command handling
ErrorCode service_execute_command(ServiceContext* service, const char* command);
ErrorCode service_get_status(ServiceContext* service, char* status_buffer, size_t buffer_size);

#endif // MYOTRONICS_SERVICE_H