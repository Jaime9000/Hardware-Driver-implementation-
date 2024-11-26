#include "service.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// HTTP server thread function declaration
static DWORD WINAPI http_server_thread(LPVOID lpParam);

ServiceContext* service_create(Config* config) {
    if (!config) return NULL;

    ServiceContext* service = (ServiceContext*)malloc(sizeof(ServiceContext));
    if (!service) return NULL;

    // Initialize members
    memset(service, 0, sizeof(ServiceContext));
    service->config = config;
    service->stop_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    service->is_running = false;
    
    // Create serial interface
    service->serial_interface = serial_interface_create(config);
    if (!service->serial_interface) {
        CloseHandle(service->stop_event);
        free(service);
        return NULL;
    }

    return service;
}

void service_destroy(ServiceContext* service) {
    if (!service) return;

    // Stop the service if it's running
    if (service->is_running) {
        service_stop(service);
    }

    // Cleanup HTTP server resources
    if (service->request_queue != NULL) {
        HttpCloseRequestQueue(service->request_queue);
    }
    if (service->url_group_id != 0) {
        HttpCloseUrlGroup(service->url_group_id);
    }
    if (service->session_id != 0) {
        HttpCloseServerSession(service->session_id);
    }

    // Cleanup other resources
    if (service->stop_event) {
        CloseHandle(service->stop_event);
    }
    if (service->serial_interface) {
        serial_interface_destroy(service->serial_interface);
    }

    free(service);
}

ErrorCode service_run(ServiceContext* service) {
    if (!service) return ERROR_DEVICE_DISCONNECTED;

    // Initialize HTTP server
    HTTPAPI_VERSION httpApiVersion = HTTPAPI_VERSION_2;
    ULONG result = HttpInitialize(httpApiVersion, HTTP_INITIALIZE_SERVER, NULL);
    if (result != NO_ERROR) {
        return ERROR_WRITE_FAILED;
    }

    // Create HTTP server session
    result = HttpCreateServerSession(httpApiVersion, &service->session_id, 0);
    if (result != NO_ERROR) {
        HttpTerminate(HTTP_INITIALIZE_SERVER, NULL);
        return ERROR_WRITE_FAILED;
    }

    // Create request queue
    result = HttpCreateRequestQueue(httpApiVersion, L"MyotronicsDriver", NULL, 0, 
                                  &service->request_queue);
    if (result != NO_ERROR) {
        return ERROR_WRITE_FAILED;
    }

    // Create URL group
    result = HttpCreateUrlGroup(service->session_id, &service->url_group_id, 0);
    if (result != NO_ERROR) {
        return ERROR_WRITE_FAILED;
    }

    // Add URLs to the group
    WCHAR url[256];
    swprintf(url, 256, L"http://localhost:%d/", DEFAULT_PORT);
    result = HttpAddUrlToUrlGroup(service->url_group_id, url, 0, 0);
    if (result != NO_ERROR) {
        return ERROR_WRITE_FAILED;
    }

    // Start HTTP server thread
    service->http_thread = CreateThread(NULL, 0, http_server_thread, service, 0, NULL);
    if (!service->http_thread) {
        return ERROR_WRITE_FAILED;
    }

    service->is_running = true;

    // Wait for stop event
    WaitForSingleObject(service->stop_event, INFINITE);

    return ERROR_NONE;
}

// HTTP server thread implementation
static DWORD WINAPI http_server_thread(LPVOID lpParam) {
    ServiceContext* service = (ServiceContext*)lpParam;
    HTTP_REQUEST_ID requestId;
    HTTP_REQUEST request;
    ULONG bytesRead;
    char buffer[4096];

    while (service->is_running) {
        // Receive a request
        HTTP_REQUEST_ID requestId;
        HTTP_REQUEST request;
        ULONG bytesRead;

        result = HttpReceiveHttpRequest(
            service->request_queue,
            HTTP_NULL_ID,
            0,
            &request,
            sizeof(HTTP_REQUEST),
            &bytesRead,
            NULL
        );

        if (result == NO_ERROR) {
            service_handle_request(service, &request);
        }
        else if (result == ERROR_MORE_DATA) {
            // Handle larger requests if needed
            // ...
        }
        else {
            Sleep(100);  // Prevent tight loop
        }
    }

    return 0;
}

// ... (more implementation details)