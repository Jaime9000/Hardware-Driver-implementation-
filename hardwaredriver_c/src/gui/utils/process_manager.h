#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

// System headers
#include <windows.h>
#include <stdbool.h>

// Project headers
#include "src/core/error_codes.h"

/**
 * @brief Shared memory structure for inter-process communication
 */
typedef struct {
    HANDLE mapping_handle;        // Handle to shared memory mapping
    void* mapped_view;           // Pointer to mapped memory view
    char* patient_name;          // Current patient name
    bool sweep_data_ready;       // Flag indicating sweep data status
    bool exit_thread;            // Flag for thread termination
    HANDLE* process_list;        // List of process handles
    size_t process_list_size;    // Number of active processes
} ProcessNamespace;

// Forward declaration of ProcessManager structure
typedef struct ProcessManager ProcessManager;

/**
 * @brief Function pointer type for process callbacks
 */
typedef void (*ProcessFunction)(ProcessManager*, void*);
// Constructor/Destructor
/**
 * @brief Creates a new process manager instance
 * 
 * @param existing_namespace Optional existing namespace to use (NULL for new)
 * @return ProcessManager* New instance or NULL on failure
 */
ProcessManager* process_manager_create(ProcessNamespace* existing_namespace);

/**
 * @brief Destroys a process manager instance and cleans up resources
 * 
 * @param manager Process manager to destroy
 */
void process_manager_destroy(ProcessManager* manager);
// Process management
/**
 * @brief Starts a new process with the given function
 * 
 * @param manager Process manager instance
 * @param func Function to execute in new process
 * @param args Arguments to pass to function
 * @return ErrorCode ERROR_NONE on success, error code otherwise
 */
ErrorCode process_manager_start_process(ProcessManager* manager, 
                                      ProcessFunction func, 
                                      void* args);

/**
 * @brief Stops a specific process
 * 
 * @param manager Process manager instance
 * @param process Handle to process to stop
 * @return ErrorCode ERROR_NONE on success, error code otherwise
 */
ErrorCode process_manager_stop_process(ProcessManager* manager, HANDLE process);

/**
 * @brief Stops all managed processes
 * 
 * @param manager Process manager instance
 * @return ErrorCode ERROR_NONE on success, error code otherwise
 */
ErrorCode process_manager_stop_all_processes(ProcessManager* manager);
// Namespace access
/**
 * @brief Gets the shared namespace
 * 
 * @param manager Process manager instance
 * @return ProcessNamespace* Pointer to namespace or NULL if not available
 */
ProcessNamespace* process_manager_get_namespace(ProcessManager* manager);
// Signal handling
/**
 * @brief Sets up signal handlers for the process manager
 * 
 * @param manager Process manager instance
 */
void process_manager_setup_handlers(ProcessManager* manager);
// Sweep process management
/**
 * @brief Terminates the sweep process if running
 * 
 * @param manager Process manager instance
 * @return ErrorCode ERROR_NONE on success, error code otherwise
 */
ErrorCode process_manager_kill_sweep_process(ProcessManager* manager);

#endif // PROCESS_MANAGER_H