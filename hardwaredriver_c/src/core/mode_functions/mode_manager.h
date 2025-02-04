#ifndef MODE_MANAGER_H
#define MODE_MANAGER_H

#include <stdbool.h>
#include "src/core/mode_functions/mode_base.h"
#include "src/core/commands.h"
#include "src/core/serial_interface.h"
#include "src/core/error_codes.h"
#include "src/core/utils/process_manager.h"

// Forward declarations
typedef struct ModeManager ModeManager;
typedef struct ModeEntry ModeEntry;

// Function pointer types
typedef ErrorCode (*ModeCreateFunc)(ModeBase** mode, 
                                  SerialInterface* interface,
                                  ProcessManager* process_manager);
typedef void (*ModeDestroyFunc)(ModeBase* mode);

// Define the possible types a mode can be
typedef enum {
    MODE_TYPE_BASE,     // For ModeBaseClass derivatives
    MODE_TYPE_CONTROL   // For ControlFunctions
} ModeType;

// The actual union of possible types
typedef struct {
    ModeType type;
    union {
        ModeBase* base_mode;
        ControlFunctions* control_mode;
    };
} ControlModeType;

// Mode entry structure
struct ModeEntry {
    IOCommand command;
    ModeConfig config;
    ModeCreateFunc create;           // Function to create the mode
    ModeDestroyFunc destroy;        // Function to destroy the mode
    ControlModeType mode;           // The actual mode type union
};

// Mode manager structure
struct ModeManager {
    SerialInterface* serial_interface;
    ProcessManager* process_manager;
    NamespaceOptions* namespace;
    ModeBase* active_mode;
    ModeType active_mode_type;
    ModeEntry* mode_entries;
    size_t mode_count;
    size_t mode_capacity;
};

// Constructor/Destructor
ErrorCode mode_manager_create(ModeManager** manager, 
                            SerialInterface* interface,
                            ProcessManager* process_manager);
void mode_manager_destroy(ModeManager* manager);

// Mode registration and execution
ErrorCode mode_manager_register_mode(ModeManager* manager, 
                                   IOCommand command,
                                   ControlModeType mode_type,
                                   ModeCreateFunc create,
                                   ModeDestroyFunc destroy);

ErrorCode mode_manager_execute_command(ModeManager* manager, 
                                     IOCommand command,
                                     size_t return_size,
                                     uint8_t** data,
                                     size_t* data_size);

static ErrorCode mode_manager_return_not_connected_data(ModeManager* manager, ModeEntry* entry);

// Utility functions
ErrorCode mode_manager_get_equipment_byte(ModeManager* manager, uint8_t* byte);
bool mode_manager_is_mode_active(ModeManager* manager, IOCommand command);
void mode_manager_close(ModeManager* manager);

#endif // MODE_MANAGER_H