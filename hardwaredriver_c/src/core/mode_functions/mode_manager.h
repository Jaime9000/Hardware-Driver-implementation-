#ifndef MODE_MANAGER_H
#define MODE_MANAGER_H

#include <stdbool.h>
#include "mode.h"
#include "commands.h"
#include "serial_interface.h"
#include "error_codes.h"

// Forward declarations
typedef struct ModeManager ModeManager;
typedef struct ModeEntry ModeEntry;

// Function pointer types
typedef ErrorCode (*ModeCreateFunc)(Mode** mode, SerialInterface* interface);
typedef void (*ModeDestroyFunc)(Mode* mode);

// Mode entry structure
struct ModeEntry {
    IOCommand command;
    ModeCreateFunc create;
    ModeDestroyFunc destroy;
};

// Mode manager structure
struct ModeManager {
    SerialInterface* serial_interface;
    Mode* active_mode;
    ModeEntry* mode_entries;
    size_t mode_count;
    size_t mode_capacity;
};

// Constructor/Destructor
ErrorCode mode_manager_create(ModeManager** manager, SerialInterface* interface);
void mode_manager_destroy(ModeManager* manager);

// Mode registration and execution
ErrorCode mode_manager_register_mode(ModeManager* manager, 
                                   IOCommand command,
                                   ModeCreateFunc create,
                                   ModeDestroyFunc destroy);

ErrorCode mode_manager_execute_command(ModeManager* manager, 
                                     IOCommand command,
                                     size_t return_size,
                                     uint8_t** data,
                                     size_t* data_size);

// Utility functions
ErrorCode mode_manager_get_equipment_byte(ModeManager* manager, uint8_t* byte);
bool mode_manager_is_mode_active(ModeManager* manager, IOCommand command);
void mode_manager_close(ModeManager* manager);

#endif // MODE_MANAGER_H