#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

#include <windows.h>
#include <stdbool.h>
#include "error_codes.h"

// Shared memory structure for namespace
typedef struct {
    HANDLE mapping_handle;
    void* mapped_view;
    char* patient_name;
    bool sweep_data_ready;
    bool exit_thread;
    HANDLE* process_list;
    size_t process_list_size;
    // Add other shared data fields as needed
} ProcessNamespace;

typedef struct ProcessManager ProcessManager;
typedef void (*ProcessFunction)(ProcessManager*, void*);

// Constructor/Destructor
ProcessManager* process_manager_create(ProcessNamespace* existing_namespace);
void process_manager_destroy(ProcessManager* manager);

// Process management
ErrorCode process_manager_start_process(ProcessManager* manager, 
                                      ProcessFunction func, 
                                      void* args);
ErrorCode process_manager_stop_process(ProcessManager* manager, HANDLE process);
ErrorCode process_manager_stop_all_processes(ProcessManager* manager);

// Namespace access
ProcessNamespace* process_manager_get_namespace(ProcessManager* manager);

// Signal handling
void process_manager_setup_handlers(ProcessManager* manager);

// Sweep process management
ErrorCode process_manager_kill_sweep_process(ProcessManager* manager);

#endif // PROCESS_MANAGER_H