#include <windows.h>
#include <stdio.h>
#include "service.h"
#include "config.h"
#include "window_placement.h"

// Function to ensure required directories exist
static ErrorCode ensure_directories(void) {
    // Create base K7 directory if it doesn't exist
    if (!CreateDirectory("C:\\K7", NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
        printf("Failed to create C:\\K7 directory\n");
        return ERROR_WRITE_FAILED;
    }

    // Create subdirectories
    const char* subdirs[] = {
        "C:\\K7\\python",
        "C:\\K7\\driver_logs",
        "C:\\K7\\data",
        NULL
    };

    for (const char** dir = subdirs; *dir != NULL; dir++) {
        if (!CreateDirectory(*dir, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
            printf("Failed to create directory: %s\n", *dir);
            return ERROR_WRITE_FAILED;
        }
    }

    return ERROR_NONE;
}

int main(int argc, char* argv[]) {
    // Initialize error handling
    ErrorCode result = ERROR_NONE;

    // Ensure required directories exist
    result = ensure_directories();
    if (result != ERROR_NONE) {
        printf("Failed to create required directories\n");
        return 1;
    }

    // Create and initialize configuration
    Config* config = config_create();
    if (!config) {
        printf("Failed to create configuration\n");
        return 1;
    }

    // Initialize window placement system
    result = window_placement_init();
    if (result != ERROR_NONE) {
        printf("Failed to initialize window placement\n");
        config_destroy(config);
        return 1;
    }

    // Create service context
    ServiceContext* service = service_create(config);
    if (!service) {
        printf("Failed to create service\n");
        config_destroy(config);
        return 1;
    }

    // Run the service (this will block until service is stopped)
    result = service_run(service);

    // Cleanup
    service_destroy(service);
    config_destroy(config);

    return result == ERROR_NONE ? 0 : 1;
}