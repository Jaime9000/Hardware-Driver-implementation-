#include "src/gui/utils/process_manager.h"
#include "src/core/logger.h"

// System headers
#include <stdlib.h>
#include <signal.h>
#include <string.h>

// Constants
#define SHARED_MEM_NAME "MyotronicsDriverSharedMem"
#define MAX_PROCESSES 64
#define MAX_PROCESS_LIST_SIZE 256

/**
 * @brief Internal process manager structure
 */
struct ProcessManager {
    ProcessNamespace* namespace;     // Shared namespace
    HANDLE processes[MAX_PROCESSES]; // Local process list
    size_t process_count;           // Number of local processes
    bool is_master;                 // Master process flag
    HANDLE sweep_process;           // Handle to sweep process
};

// Forward declarations
static void cleanup_process_manager(ProcessManager* manager);
static ErrorCode terminate_process(HANDLE process);
// Signal handlers
/**
 * @brief Windows console control handler
 */
static BOOL WINAPI console_handler(DWORD signal) {
    switch (signal) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            ExitProcess(0);
            return TRUE;
    }
    return FALSE;
}

/**
 * @brief Signal handler for process termination
 */
static void signal_handler(int signum) {
    switch (signum) {
        case SIGTERM:
        case SIGINT:
            ExitProcess(0);
            break;
    }
}

ProcessManager* process_manager_create(ProcessNamespace* existing_namespace) {
    ProcessManager* manager = calloc(1, sizeof(ProcessManager));
    if (!manager) {
        log_error("Failed to allocate ProcessManager");
        return NULL;
    }

    if (existing_namespace) {
        manager->namespace = existing_namespace;
        manager->is_master = false;
    } else {
        ProcessNamespace* ns = calloc(1, sizeof(ProcessNamespace));
        if (!ns) {
            free(manager);
            log_error("Failed to allocate ProcessNamespace");
            return NULL;
        }
        
        ns->mapping_handle = CreateFileMapping(
            INVALID_HANDLE_VALUE,
            NULL,
            PAGE_READWRITE,
            0,
            sizeof(ProcessNamespace),
            SHARED_MEM_NAME
        );

        if (!ns->mapping_handle) {
            free(ns);
            free(manager);
            log_error("Failed to create file mapping");
            return NULL;
        }

        ns->mapped_view = MapViewOfFile(
            ns->mapping_handle,
            FILE_MAP_ALL_ACCESS,
            0,
            0,
            sizeof(ProcessNamespace)
        );

        if (!ns->mapped_view) {
            CloseHandle(ns->mapping_handle);
            free(ns);
            free(manager);
            log_error("Failed to map view of file");
            return NULL;
        }

        // Initialize process list in shared memory
        ns->process_list = calloc(MAX_PROCESS_LIST_SIZE, sizeof(HANDLE));
        if (!ns->process_list) {
            UnmapViewOfFile(ns->mapped_view);
            CloseHandle(ns->mapping_handle);
            free(ns);
            free(manager);
            log_error("Failed to allocate process list");
            return NULL;
        }
        ns->process_list_size = 0;

        manager->namespace = ns;
        manager->is_master = true;
        process_manager_setup_handlers(manager);
    }

    manager->sweep_process = NULL;
    return manager;
}

void process_manager_setup_handlers(ProcessManager* manager) {
    if (!manager || !manager->is_master) return;
    
    SetConsoleCtrlHandler(console_handler, TRUE);
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
}

ErrorCode process_manager_start_process(ProcessManager* manager, 
                                      ProcessFunction func, 
                                      void* args) {
    if (!manager || !func) return ERROR_INVALID_PARAM;
    if (manager->process_count >= MAX_PROCESSES) return ERROR_LIMIT_EXCEEDED;

    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);

    // Create process suspended
    if (!CreateProcess(NULL, NULL, NULL, NULL, FALSE, 
                      CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
        log_error("Failed to create process");
        return ERROR_PROCESS_CREATE;
    }

    // Store process handle
    manager->processes[manager->process_count++] = pi.hProcess;
    
    // Add to shared process list
    if (manager->namespace) {
        manager->namespace->process_list[manager->namespace->process_list_size++] = pi.hProcess;
    }

    // Resume the process
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);

    return ERROR_NONE;
}

ErrorCode process_manager_stop_process(ProcessManager* manager, HANDLE process) {
    if (!manager || !process) return ERROR_INVALID_PARAM;

    ErrorCode result = terminate_process(process);
    if (result != ERROR_NONE) return result;

    // Remove from local process list
    for (size_t i = 0; i < manager->process_count; i++) {
        if (manager->processes[i] == process) {
            // Shift remaining processes left
            memmove(&manager->processes[i], 
                   &manager->processes[i + 1],
                   (manager->process_count - i - 1) * sizeof(HANDLE));
            manager->process_count--;
            break;
        }
    }

    // Remove from shared process list
    if (manager->namespace) {
        for (size_t i = 0; i < manager->namespace->process_list_size; i++) {
            if (manager->namespace->process_list[i] == process) {
                memmove(&manager->namespace->process_list[i],
                       &manager->namespace->process_list[i + 1],
                       (manager->namespace->process_list_size - i - 1) * sizeof(HANDLE));
                manager->namespace->process_list_size--;
                break;
            }
        }
    }

    return ERROR_NONE;
}

ErrorCode process_manager_stop_all_processes(ProcessManager* manager) {
    if (!manager) return ERROR_INVALID_PARAM;

    ErrorCode result = ERROR_NONE;
    
    // Stop all processes in local list
    for (size_t i = 0; i < manager->process_count; i++) {
        ErrorCode temp_result = terminate_process(manager->processes[i]);
        if (temp_result != ERROR_NONE) result = temp_result;
    }
    manager->process_count = 0;

    // Stop all processes in shared list if master
    if (manager->is_master && manager->namespace) {
        for (size_t i = 0; i < manager->namespace->process_list_size; i++) {
            ErrorCode temp_result = terminate_process(manager->namespace->process_list[i]);
            if (temp_result != ERROR_NONE) result = temp_result;
        }
        manager->namespace->process_list_size = 0;
    }

    return result;
}

ErrorCode process_manager_kill_sweep_process(ProcessManager* manager) {
    if (!manager) return ERROR_INVALID_PARAM;
    
    if (manager->sweep_process) {
        ErrorCode result = terminate_process(manager->sweep_process);
        if (result == ERROR_NONE) {
            CloseHandle(manager->sweep_process);
            manager->sweep_process = NULL;
        }
        return result;
    }
    return ERROR_NONE;
}

ProcessNamespace* process_manager_get_namespace(ProcessManager* manager) {
    return manager ? manager->namespace : NULL;
}

static ErrorCode terminate_process(HANDLE process) {
    if (!TerminateProcess(process, 0)) {
        log_error("Failed to terminate process");
        return ERROR_PROCESS_TERMINATE;
    }
    CloseHandle(process);
    return ERROR_NONE;
}

static void cleanup_process_manager(ProcessManager* manager) {
    if (!manager) return;

    process_manager_stop_all_processes(manager);
    process_manager_kill_sweep_process(manager);

    if (manager->is_master && manager->namespace) {
        if (manager->namespace->process_list) {
            free(manager->namespace->process_list);
        }
        if (manager->namespace->mapped_view) {
            UnmapViewOfFile(manager->namespace->mapped_view);
        }
        if (manager->namespace->mapping_handle) {
            CloseHandle(manager->namespace->mapping_handle);
        }
        free(manager->namespace);
    }
}

void process_manager_destroy(ProcessManager* manager) {
    if (!manager) return;
    
    cleanup_process_manager(manager);
    free(manager);
}