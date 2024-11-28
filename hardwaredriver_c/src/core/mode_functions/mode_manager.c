#include <stdlib.h>
#include <string.h>
#include "mode_manager.h"
#include "logger.h"
#include "control_functions.h"

#define INITIAL_MODE_CAPACITY 32
#define MAX_MODE_RETRIES 5

static ErrorCode resize_mode_entries(ModeManager* manager) {
    if (!manager) {
        set_last_error(ERROR_INVALID_PARAMETER);
        log_error("Invalid manager parameter in resize_mode_entries");
        return ERROR_INVALID_PARAMETER;
    }

    size_t new_capacity = manager->mode_capacity * 2;
    log_debug("Resizing mode entries from %zu to %zu", manager->mode_capacity, new_capacity);
    
    ModeEntry* new_entries = realloc(manager->mode_entries, new_capacity * sizeof(ModeEntry));
    if (!new_entries) {
        set_last_error(ERROR_MEMORY_ALLOCATION);
        log_error("Failed to allocate memory for mode entries resize");
        return ERROR_MEMORY_ALLOCATION;
    }
    
    manager->mode_entries = new_entries;
    manager->mode_capacity = new_capacity;
    
    log_debug("Mode entries resized successfully");
    set_last_error(ERROR_NONE);
    return ERROR_NONE;
}

ErrorCode mode_manager_create(ModeManager** manager, SerialInterface* interface) {
    if (!manager || !interface) {
        set_last_error(ERROR_INVALID_PARAMETER);
        log_error("Invalid parameters in mode_manager_create");
        return ERROR_INVALID_PARAMETER;
    }

    log_debug("Creating new mode manager");
    ModeManager* new_manager = calloc(1, sizeof(ModeManager));
    if (!new_manager) {
        set_last_error(ERROR_MEMORY_ALLOCATION);
        log_error("Failed to allocate memory for mode manager");
        return ERROR_MEMORY_ALLOCATION;
    }

    new_manager->mode_entries = calloc(INITIAL_MODE_CAPACITY, sizeof(ModeEntry));
    if (!new_manager->mode_entries) {
        free(new_manager);
        set_last_error(ERROR_MEMORY_ALLOCATION);
        log_error("Failed to allocate memory for mode entries");
        return ERROR_MEMORY_ALLOCATION;
    }

    new_manager->serial_interface = interface;
    new_manager->mode_capacity = INITIAL_MODE_CAPACITY;
    new_manager->mode_count = 0;
    new_manager->active_mode = NULL;

    *manager = new_manager;
    log_debug("Mode manager created successfully");
    set_last_error(ERROR_NONE);
    return ERROR_NONE;
}

void mode_manager_destroy(ModeManager* manager) {
    if (!manager) {
        log_warning("Attempted to destroy NULL mode manager");
        return;
    }

    log_debug("Destroying mode manager");
    if (manager->active_mode) {
        mode_destroy(manager->active_mode);
    }

    free(manager->mode_entries);
    free(manager);
    log_debug("Mode manager destroyed successfully");
}

ErrorCode mode_manager_register_mode(ModeManager* manager, 
                                   IOCommand command,
                                   ModeCreateFunc create,
                                   ModeDestroyFunc destroy) {
    if (!manager || !create) {
        set_last_error(ERROR_INVALID_PARAMETER);
        log_error("Invalid parameters in mode_manager_register_mode");
        return ERROR_INVALID_PARAMETER;
    }

    log_debug("Registering mode for command: %d", command);

    if (manager->mode_count >= manager->mode_capacity) {
        ErrorCode result = resize_mode_entries(manager);
        if (result != ERROR_NONE) {
            set_last_error(result);
            log_error("Failed to resize mode entries: %s", get_error_string(result));
            return result;
        }
    }

    // Check for duplicate command
    for (size_t i = 0; i < manager->mode_count; i++) {
        if (manager->mode_entries[i].command == command) {
            set_last_error(ERROR_DUPLICATE_COMMAND);
            log_error("Duplicate command registration attempted: %d", command);
            return ERROR_DUPLICATE_COMMAND;
        }
    }

    manager->mode_entries[manager->mode_count].command = command;
    manager->mode_entries[manager->mode_count].create = create;
    manager->mode_entries[manager->mode_count].destroy = destroy;
    manager->mode_count++;

    log_debug("Mode registered successfully");
    set_last_error(ERROR_NONE);
    return ERROR_NONE;
}

static ModeEntry* find_mode_entry(ModeManager* manager, IOCommand command) {
    if (!manager) {
        log_error("Invalid manager parameter in find_mode_entry");
        return NULL;
    }

    for (size_t i = 0; i < manager->mode_count; i++) {
        if (manager->mode_entries[i].command == command) {
            return &manager->mode_entries[i];
        }
    }
    
    log_debug("Mode entry not found for command: %d", command);
    return NULL;
}

static ErrorCode change_active_mode(ModeManager* manager, ModeEntry* entry) {
    if (!manager || !entry) {
        set_last_error(ERROR_INVALID_PARAMETER);
        log_error("Invalid parameters in change_active_mode");
        return ERROR_INVALID_PARAMETER;
    }

    if (manager->active_mode) {
        Mode* current_mode = manager->active_mode;
        if (current_mode->vtable->get_mode_number(current_mode) == entry->command) {
            log_debug("Already in correct mode: %d", entry->command);
            set_last_error(ERROR_NONE);
            return ERROR_NONE;
        }
        
        log_debug("Destroying current mode before change");
        mode_destroy(current_mode);
        manager->active_mode = NULL;
    }

    log_debug("Creating new mode for command: %d", entry->command);
    Mode* new_mode;
    ErrorCode result = entry->create(&new_mode, manager->serial_interface);
    if (result != ERROR_NONE) {
        set_last_error(result);
        log_error("Failed to create new mode: %s", get_error_string(result));
        return result;
    }

    manager->active_mode = new_mode;
    log_debug("Mode changed successfully");
    set_last_error(ERROR_NONE);
    return ERROR_NONE;
}

ErrorCode mode_manager_execute_command(ModeManager* manager, 
                                     IOCommand command,
                                     size_t return_size,
                                     uint8_t** data,
                                     size_t* data_size) {
    if (!manager || !data || !data_size) {
        set_last_error(ERROR_INVALID_PARAMETER);
        log_error("Invalid parameters in mode_manager_execute_command");
        return ERROR_INVALID_PARAMETER;
    }

    log_debug("Executing command: %d", command);
    *data = NULL;
    *data_size = 0;

    ModeEntry* entry = find_mode_entry(manager, command);
    if (!entry) {
        set_last_error(ERROR_INVALID_COMMAND);
        log_error("Invalid command: %d", command);
        return ERROR_INVALID_COMMAND;
    }

    // Handle equipment byte request specially
    if (command == CMD_GET_EQUIPMENT_BYTE && manager->active_mode) {
        log_debug("Handling equipment byte request");
        uint8_t* byte_data = malloc(1);
        if (!byte_data) {
            set_last_error(ERROR_MEMORY_ALLOCATION);
            log_error("Failed to allocate memory for equipment byte");
            return ERROR_MEMORY_ALLOCATION;
        }
        *byte_data = mode_get_device_byte(manager->active_mode);
        *data = byte_data;
        *data_size = 1;
        log_debug("Equipment byte retrieved: %d", *byte_data);
        set_last_error(ERROR_NONE);
        return ERROR_NONE;
    }

    ErrorCode result = change_active_mode(manager, entry);
    if (result != ERROR_NONE) {
        set_last_error(result);
        log_error("Failed to change mode: %s", get_error_string(result));
        return result;
    }

    // Execute mode with retries
    uint8_t* mode_data = NULL;
    size_t mode_data_size = 0;
    
    for (int retry = 0; retry < MAX_MODE_RETRIES; retry++) {
        log_debug("Executing mode, attempt %d/%d", retry + 1, MAX_MODE_RETRIES);
        result = mode_execute(manager->active_mode, false);
        if (result == ERROR_NONE) {
            if (return_size > 0) {
                mode_data = malloc(return_size);
                if (!mode_data) {
                    set_last_error(ERROR_MEMORY_ALLOCATION);
                    log_error("Failed to allocate memory for mode data");
                    return ERROR_MEMORY_ALLOCATION;
                }
                // Copy data from mode's buffer to our allocated buffer
                memcpy(mode_data, /* mode's buffer */, return_size);
                mode_data_size = return_size;
                log_debug("Mode data copied successfully, size: %zu", mode_data_size);
            }
            break;
        }
        
        if (retry < MAX_MODE_RETRIES - 1) {
            log_warning("Mode execution failed, retrying (attempt %d/%d): %s", 
                       retry + 2, MAX_MODE_RETRIES, get_error_string(result));
        }
    }

    if (result != ERROR_NONE) {
        set_last_error(result);
        log_error("Mode execution failed after %d attempts: %s", 
                 MAX_MODE_RETRIES, get_error_string(result));
        return result;
    }

    *data = mode_data;
    *data_size = mode_data_size;
    log_debug("Command executed successfully");
    set_last_error(ERROR_NONE);
    return ERROR_NONE;
}

ErrorCode mode_manager_get_equipment_byte(ModeManager* manager, uint8_t* byte) {
    if (!manager || !byte) {
        set_last_error(ERROR_INVALID_PARAMETER);
        log_error("Invalid parameters in mode_manager_get_equipment_byte");
        return ERROR_INVALID_PARAMETER;
    }

    if (!manager->active_mode) {
        set_last_error(ERROR_NO_ACTIVE_MODE);
        log_error("No active mode when requesting equipment byte");
        return ERROR_NO_ACTIVE_MODE;
    }

    *byte = mode_get_device_byte(manager->active_mode);
    log_debug("Equipment byte retrieved: %d", *byte);
    set_last_error(ERROR_NONE);
    return ERROR_NONE;
}

bool mode_manager_is_mode_active(ModeManager* manager, IOCommand command) {
    if (!manager || !manager->active_mode) {
        log_debug("No active mode when checking mode status");
        return false;
    }

    ModeEntry* entry = find_mode_entry(manager, command);
    if (!entry) {
        log_debug("Mode entry not found for command: %d", command);
        return false;
    }

    bool is_active = mode_get_number(manager->active_mode) == command;
    log_debug("Mode %d active status: %s", command, is_active ? "true" : "false");
    return is_active;
}

void mode_manager_close(ModeManager* manager) {
    if (!manager) {
        log_warning("Attempted to close NULL mode manager");
        return;
    }

    log_debug("Closing mode manager");
    if (manager->active_mode) {
        mode_destroy(manager->active_mode);
        manager->active_mode = NULL;
    }

    if (manager->serial_interface) {
        serial_interface_close(manager->serial_interface);
    }
    log_debug("Mode manager closed successfully");
}