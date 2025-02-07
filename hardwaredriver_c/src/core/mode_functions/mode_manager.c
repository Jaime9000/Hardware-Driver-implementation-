#include "src/core/mode_functions/mode_manager.h"
#include "src/core/logger.h"
#include "src/core/mode_functions/control_functions.h"

// Mode includes with full paths
#include "src/core/mode_functions/mode_0.h"
#include "src/core/mode_functions/mode_42.h"
#include "src/core/mode_functions/mode_43.h"
#include "src/core/mode_functions/mode_44.h"
#include "src/core/mode_functions/mode_44_sweep_scan.h"
#include "src/core/mode_functions/mode_51.h"
#include "src/core/mode_functions/mode_52.h"
#include "src/core/mode_functions/mode_53.h"
#include "src/core/mode_functions/mode_56.h"
#include "src/core/mode_functions/mode_57.h"
#include "src/core/mode_functions/mode_sweep.h"
#include "src/core/mode_functions/emg_version.h"

// System headers
#include <stdlib.h>
#include <string.h>

#define INITIAL_MODE_CAPACITY 32
#define MAX_MODE_RETRIES 5

static ErrorCode resize_mode_entries(ModeManager* manager) {
    if (!manager) {
        set_last_error(ERROR_INVALID_PARAM);
        log_error("Invalid manager parameter in resize_mode_entries");
        return ERROR_INVALID_PARAM;
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

ErrorCode mode_manager_create(ModeManager** manager, 
                            SerialInterface* interface,
                            ProcessManager* process_manager) {
    if (!manager || !interface || !process_manager) {
        set_last_error(ERROR_INVALID_PARAM);
        log_error("Invalid parameters in mode_manager_create");
        return ERROR_INVALID_PARAM;
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

    new_manager->namespace = process_manager_get_namespace(process_manager);
    new_manager->process_manager = process_manager_create(new_manager->namespace);
    
    /*
    → main.py (creates ProcessManager)
        → service.py (receives ProcessManager, passes to USBControl)
            → modes.py (creates new ProcessManager with shared namespace)
    */
    new_manager->mode_capacity = INITIAL_MODE_CAPACITY;
    new_manager->mode_count = 0;
    new_manager->active_mode = NULL;
    new_manager->active_mode_type = NULL;
    new_manager->mode_entries = NULL;
    
    

    // Variables for command parsing
    IOCommand cmd;
    ModeConfig* mode_cfg;
    ErrorCode result;
    
    // Base mode type
    ControlModeType base_mode = { .type = MODE_TYPE_BASE };

    // Register mode 0 variants
    if (parse_command(CMD_MODE_0_CONF, &cmd, &mode_cfg) == ERROR_NONE) {
        mode_manager_register_mode(new_manager, NULL, mode_cfg, base_mode, mode_zero_create, mode_zero_destroy);
    }
    if (parse_command(CMD_MODE_0_RAW, &cmd, &mode_cfg) == ERROR_NONE) {
        mode_manager_register_mode(new_manager, NULL, mode_cfg, base_mode, mode_zero_raw_create, mode_zero_raw_destroy);
    }
    if (parse_command(CMD_MODE_0_ALIGN, &cmd, &mode_cfg) == ERROR_NONE) {
        mode_manager_register_mode(new_manager, NULL, mode_cfg, base_mode, mode_zero_align_create, mode_zero_align_destroy);
    }

    // Register mode 42 variants
    if (parse_command(CMD_MODE_42_RAW, &cmd, &mode_cfg) == ERROR_NONE) {
        mode_manager_register_mode(new_manager, NULL, mode_cfg, base_mode, mode_42_raw_create, mode_42_raw_destroy);
    }
    if (parse_command(CMD_MODE_42_RAW_Q, &cmd, &mode_cfg) == ERROR_NONE) {
        mode_manager_register_mode(new_manager, NULL, mode_cfg, base_mode, mode_42_raw_notch_q_create, mode_42_raw_notch_q_destroy);
    }
    if (parse_command(CMD_MODE_42_RAW_S, &cmd, &mode_cfg) == ERROR_NONE) {
        mode_manager_register_mode(new_manager, NULL, mode_cfg, base_mode, mode_42_raw_notch_s_create, mode_42_raw_notch_s_destroy);
    }
    if (parse_command(CMD_MODE_42_RAW_U, &cmd, &mode_cfg) == ERROR_NONE) {
        mode_manager_register_mode(new_manager, NULL, mode_cfg, base_mode, mode_42_raw_notch_u_create, mode_42_raw_notch_u_destroy);
    }
    if (parse_command(CMD_MODE_42_RAW_W, &cmd, &mode_cfg) == ERROR_NONE) {
        mode_manager_register_mode(new_manager, NULL, mode_cfg, base_mode, mode_42_raw_notch_w_create, mode_42_raw_notch_w_destroy);
    }
    if (parse_command(CMD_MODE_42_RAW_T, &cmd, &mode_cfg) == ERROR_NONE) {
        mode_manager_register_mode(new_manager, NULL, mode_cfg, base_mode, mode_42_raw_notch_t_create, mode_42_raw_notch_t_destroy);
    }
    if (parse_command(CMD_MODE_42_RAW_V, &cmd, &mode_cfg) == ERROR_NONE) {
        mode_manager_register_mode(new_manager, NULL, mode_cfg, base_mode, mode_42_raw_notch_v_create, mode_42_raw_notch_v_destroy);
    }
    if (parse_command(CMD_MODE_42_RAW_P, &cmd, &mode_cfg) == ERROR_NONE) {
        mode_manager_register_mode(new_manager, NULL, mode_cfg, base_mode, mode_42_raw_notch_p_create, mode_42_raw_notch_p_destroy);
    }
    if (parse_command(CMD_MODE_42_RAW_R, &cmd, &mode_cfg) == ERROR_NONE) {
        mode_manager_register_mode(new_manager, NULL, mode_cfg, base_mode, mode_42_raw_notch_r_create, mode_42_raw_notch_r_destroy);
    }

    // Register mode 43 variants
    if (parse_command(CMD_MODE_43_RAW, &cmd, &mode_cfg) == ERROR_NONE) {
        mode_manager_register_mode(new_manager, NULL, mode_cfg, base_mode, mode_43_raw_create, mode_43_raw_destroy);
    }
    if (parse_command(CMD_MODE_43_RAW_Q, &cmd, &mode_cfg) == ERROR_NONE) {
        mode_manager_register_mode(new_manager, NULL, mode_cfg, base_mode, mode_43_raw_notch_q_create, mode_43_raw_notch_q_destroy);
    }
    if (parse_command(CMD_MODE_43_RAW_S, &cmd, &mode_cfg) == ERROR_NONE) {
        mode_manager_register_mode(new_manager, NULL, mode_cfg, base_mode, mode_43_raw_notch_s_create, mode_43_raw_notch_s_destroy);
    }
    if (parse_command(CMD_MODE_43_RAW_U, &cmd, &mode_cfg) == ERROR_NONE) {
        mode_manager_register_mode(new_manager, NULL, mode_cfg, base_mode, mode_43_raw_notch_u_create, mode_43_raw_notch_u_destroy);
    }
    if (parse_command(CMD_MODE_43_RAW_W, &cmd, &mode_cfg) == ERROR_NONE) {
        mode_manager_register_mode(new_manager, NULL, mode_cfg, base_mode, mode_43_raw_notch_w_create, mode_43_raw_notch_w_destroy);
    }
    if (parse_command(CMD_MODE_43_RAW_T, &cmd, &mode_cfg) == ERROR_NONE) {
        mode_manager_register_mode(new_manager, NULL, mode_cfg, base_mode, mode_43_raw_notch_t_create, mode_43_raw_notch_t_destroy);
    }
    if (parse_command(CMD_MODE_43_RAW_V, &cmd, &mode_cfg) == ERROR_NONE) {
        mode_manager_register_mode(new_manager, NULL, mode_cfg, base_mode, mode_43_raw_notch_v_create, mode_43_raw_notch_v_destroy);
    }
    if (parse_command(CMD_MODE_43_RAW_P, &cmd, &mode_cfg) == ERROR_NONE) {
        mode_manager_register_mode(new_manager, NULL, mode_cfg, base_mode, mode_43_raw_notch_p_create, mode_43_raw_notch_p_destroy);
    }
    if (parse_command(CMD_MODE_43_RAW_R, &cmd, &mode_cfg) == ERROR_NONE) {
        mode_manager_register_mode(new_manager, NULL, mode_cfg, base_mode, mode_43_raw_notch_r_create, mode_43_raw_notch_r_destroy);
    }
    if (parse_command(CMD_MODE_43_EMG, &cmd, &mode_cfg) == ERROR_NONE) {
        mode_manager_register_mode(new_manager, NULL, mode_cfg, base_mode, mode_43_raw_emg_create, mode_43_raw_emg_destroy);
    }

    // Register mode 44 variants
    if (parse_command(CMD_MODE_44_RAW, &cmd, &mode_cfg) == ERROR_NONE) {
        mode_manager_register_mode(new_manager, NULL, mode_cfg, base_mode, mode_44_raw_create, mode_44_raw_destroy);
    }
    if (parse_command(CMD_MODE_44_RAW_NO_IMAGE, &cmd, &mode_cfg) == ERROR_NONE) {
        mode_manager_register_mode(new_manager, NULL, mode_cfg, base_mode, mode_44_raw_no_image_create, mode_44_raw_no_image_destroy);
    }
    if (parse_command(CMD_MODE_44_SWEEP, &cmd, &mode_cfg) == ERROR_NONE) {
        mode_manager_register_mode(new_manager, NULL, mode_cfg, base_mode, mode_44_sweep_create, mode_44_sweep_destroy);
    }

    // Register other modes
    if (parse_command(CMD_MODE_51_RAW, &cmd, &mode_cfg) == ERROR_NONE) {
        mode_manager_register_mode(new_manager, NULL, mode_cfg, base_mode, mode_51_raw_create, mode_51_raw_destroy);
    }
    if (parse_command(CMD_MODE_52_RAW, &cmd, &mode_cfg) == ERROR_NONE) {
        mode_manager_register_mode(new_manager, NULL, mode_cfg, base_mode, mode_52_raw_create, mode_52_raw_destroy);
    }
    if (parse_command(CMD_MODE_53_RAW, &cmd, &mode_cfg) == ERROR_NONE) {
        mode_manager_register_mode(new_manager, NULL, mode_cfg, base_mode, mode_53_raw_create, mode_53_raw_destroy);
    }
    if (parse_command(CMD_MODE_56_RAW, &cmd, &mode_cfg) == ERROR_NONE) {
        mode_manager_register_mode(new_manager, NULL, mode_cfg, base_mode, mode_56_raw_create, mode_56_raw_destroy);
    }
    if (parse_command(CMD_MODE_57_RAW, &cmd, &mode_cfg) == ERROR_NONE) {
        mode_manager_register_mode(new_manager, NULL, mode_cfg, base_mode, mode_57_raw_create, mode_57_raw_destroy);
    }
    if (parse_command(CMD_MODE_57_RAW_NO_IMAGE, &cmd, &mode_cfg) == ERROR_NONE) {
        mode_manager_register_mode(new_manager, NULL, mode_cfg, base_mode, mode_57_raw_no_image_create, mode_57_raw_no_image_destroy);
    }

    // Register special modes
    if (parse_command(CMD_EMG_VERSION, &cmd, &mode_cfg) == ERROR_NONE) {
        mode_manager_register_mode(new_manager, NULL, mode_cfg, base_mode, emg_version_create, emg_version_destroy);
    }
    if (parse_command(CMD_CHECK_CONNECTION, &cmd, &mode_cfg) == ERROR_NONE) {
        mode_manager_register_mode(new_manager, NULL, mode_cfg, base_mode, check_connection_create, check_connection_destroy);
    }

    // Control mode type
    ControlModeType control_mode = { .type = MODE_TYPE_CONTROL };

    // Register control functions using IOCommand enum
    static const IOCommand control_commands[] = {
        RTS_ON,
        RTS_OFF,
        DTR_ON,
        DTR_OFF,
        RESET_HARDWARE_60,
        RESET_HARDWARE_50,
        DEVICE_STATUSES
    };

    for (size_t i = 0; i < sizeof(control_commands) / sizeof(IOCommand); i++) {
        mode_manager_register_mode(new_manager, control_commands[i], NULL, control_mode, NULL, NULL);
    }

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
        mode_base_destroy(manager->active_mode);
    }

    free(manager->mode_entries);
    free(manager);
    log_debug("Mode manager destroyed successfully");
}

ErrorCode mode_manager_register_mode(ModeManager* manager, 
                                   IOCommand command,
                                   ModeConfig* config,
                                   ControlModeType mode_type,
                                   ModeCreateFunc create,
                                   ModeDestroyFunc destroy) {
    if (!manager || !create) {
        set_last_error(ERROR_INVALID_PARAM);
        log_error("Invalid parameters in mode_manager_register_mode");
        return ERROR_INVALID_PARAM;
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
    // Set up the new entry
    ModeEntry* entry = &manager->mode_entries[manager->mode_count];
    entry->command = command;
    entry->config = config;
    entry->create = create;
    entry->destroy = destroy;
    entry->mode = mode_type;
    
    manager->mode_count++;

    log_debug("Mode registered successfully");
    set_last_error(ERROR_NONE);
    return ERROR_NONE;
}

static ModeEntry* find_mode_entry(ModeManager* manager, IOCommand command, ModeConfig* config ) {
    if (!manager) {
        log_error("Invalid manager parameter in find_mode_entry");
        return NULL;
    }

    for (size_t i = 0; i < manager->mode_count; i++) {
        if (manager->mode_entries[i].command == command && manager->mode_entries[i].config == config) {
            return &manager->mode_entries[i];
        }
    }
    
    log_debug("Mode entry not found for command: %d", command);
    return NULL;
}

static ErrorCode change_active_mode(ModeManager* manager, ModeEntry* entry) {
    if (!manager || !entry) {
        set_last_error(ERROR_INVALID_PARAM);
        log_error("Invalid parameters in change_active_mode");
        return ERROR_INVALID_PARAM;
    }

    /*
    // Check if current mode is active and needs special handling
    if (manager->active_mode) {
        ModeBase* current_mode = manager->active_mode;
        
        // Check if we're already in the correct mode
        if (current_mode->vtable->get_mode_number(current_mode) == entry->command) {
            log_debug("Already in correct mode: %d", entry->command);
            set_last_error(ERROR_NONE);
            return ERROR_NONE;
        }
    */
        if (manager->active_mode_type != entry->mode) {
                // Mode change needed
                if (manager->active_mode && mode_base_is_sweep(manager->active_mode)) {
                    mode_base_stop(manager->active_mode);
                }

                if(entry->mode.type != MODE_TYPE_BASE){
                    log_error("Mode type is not base");
                    return ERROR_INVALID_PARAM;
                    log_debug("Creating new mode for config: %d", entry->config);

                    log_debug("Destroying current mode before change");
                    mode_base_destroy(curraent_mode);
                    manager->active_mode = NULL;

                    ModeBase* new_mode;
                    ErrorCode result = entry->create(&new_mode, 
                                   manager->serial_interface,
                                   manager->process_manager);
                    if (result != ERROR_NONE) {
                        set_last_error(result);
                        log_error("Failed to create new mode: %s", get_error_string(result));
                        return result;
                    }
                    manager->active_mode = new_mode;
                    log_debug("Mode changed successfully");
                    set_last_error(ERROR_NONE);
                    return ERROR_NONE;
                    //else if(entry->mode.type == MODE_TYPE_CONTROL){
                    //manger->active_mode = control_functions_init(manager->active_mode, manager->serial_interface);
                    //manger->active_mode = mode_base_create(manager->active_mode, manager->serial_interface, manager->process_manager, entry->mode.base_mode->vtable, entry->mode.base_mode->impl);
                }
                
            }
    }


static ErrorCode handle_mode_data(ModeBase* mode,
                                bool disconnected,
                                size_t return_size,
                                uint8_t** data,
                                size_t* data_size) {
    ErrorCode result;
    uint8_t* mode_data = NULL;
    size_t actual_size = 0;  // Track actual data size from mode execution
    ErrorCode last_error = ERROR_NONE;  // Track last error for logging
    
    // Try execution up to MAX_MODE_RETRIES times
    for (int retry = 0; retry < MAX_MODE_RETRIES; retry++) {
        // Execute mode and get data
        result = mode_base_execute(mode, &mode_data, &actual_size, disconnected);
        
        if (result == ERROR_NONE && mode_data != NULL) {
            // Handle return size slicing
            if (return_size > 0 && return_size < actual_size) {
                // Allocate and copy only requested size
                uint8_t* sized_data = malloc(return_size);
                if (!sized_data) {
                    free(mode_data);  // Clean up original data
                    return ERROR_MEMORY_ALLOCATION;
                }
                memcpy(sized_data, mode_data, return_size);
                free(mode_data);  // Free original larger buffer
                mode_data = sized_data;
                actual_size = return_size;
            }
            
            // Set output parameters
            *data = mode_data;
            *data_size = actual_size;
            return ERROR_NONE;
        }
        
        // Handle errors
        if (result == ERROR_SERIAL_EXCEPTION) {
            log_error("Serial exception in mode execution");
            last_error = result;
            return result;  // Immediate return for serial exceptions
        }
        
        // Log non-serial errors
        if (result != ERROR_NONE) {
            log_error("Mode execution failed (attempt %d/%d): %s", 
                     retry + 1, MAX_MODE_RETRIES, 
                     get_error_string(result));
            last_error = result;
        }

        // Try handshake before retry
        if (retry < MAX_MODE_RETRIES - 1) {
            ErrorCode handshake_result = mode_base_handshake(mode);
            if (handshake_result != ERROR_NONE) {
                log_error("Handshake failed during retry: %s",
                         get_error_string(handshake_result));
            }
        }
    }

    // If we got here, all retries failed
    set_last_error(last_error);  // Store last error for global access
    return last_error;
}

static ErrorCode return_not_connected_data(ModeManager* manager, IOCommand command) {
    if (!manager) {
        return ERROR_INVALID_PARAM;
    }

    ModeEntry* entry = find_mode_entry(manager, command);
    if (!entry) {
        set_last_error(ERROR_INVALID_COMMAND);
        log_error("Invalid command: %d", command);
        return ERROR_INVALID_COMMAND;
    }

    if (entry->mode.type == MODE_TYPE_BASE) {
        // Handle base mode execution when disconnected
        ErrorCode result = change_active_mode(manager, entry);
        if (result != ERROR_NONE) {
            uint8_t output[1024];  // Buffer for output data
            size_t output_length;
        
        // Execute with disconnected=true
            return mode_base_execute(manager->active_mode, output, &output_length, true);
        }

        
    }
    else if (entry->mode.type == MODE_TYPE_CONTROL) {
        // Handle control functions when disconnected
        ControlFunctions* control = control_functions_init(control, manager->serial_interface);
        ErrorCode result = control_functions_execute(control, entry->command, true);  // true for disconnected
        control_functions_destroy(control);
        return result;
    }

    return ERROR_INVALID_MODE_TYPE;
}



ErrorCode mode_manager_execute_command(ModeManager* manager, IOCommand command) {
    if (!manager) {
        return ERROR_INVALID_PARAM;
    }


    ModeEntry* entry = find_mode_entry(manager, command);
    if (!entry) {
        set_last_error(ERROR_INVALID_COMMAND);
        log_error("Invalid command: %d", command);
        return ERROR_INVALID_COMMAND;
    }
    
    if (entry->mode.type == MODE_TYPE_BASE) {
        // Handle base mode execution
        // Handle equipment byte request specially
        if (command == CMD_GET_EQUIPMENT_BYTE && manager->active_mode) {
            log_debug("Handling equipment byte request");
            manager->active_mode->device_byte = mode_base_get_device_byte(manager->active_mode);
                      // Set size to 1 byte
            return ERROR_NONE;
        }
        change_active_mode(manager, entry);
        uint8_t* mode_data = NULL;
        size_t actual_size = 0;
        size_t rsize = 0;


        ErrorCode result = handle_mode_data(manager->active_mode, false, &return_size, &mode_data, &actual_size);
        if (result != ERROR_NONE) {
            *data = mode_data;
            *data_size = actual_size;
            *r_size = return_size;
            return result;
        }
    }

    if (entry->mode.type == MODE_TYPE_CONTROL) {
        ControlFunctions* control = control_functions_init(control, manager->serial_interface, false);
        result =control_functions_execute(control, entry->command, false);
        control_functions_destroy(control);
    }
        
        
    if (result == ERROR_SERIAL_EXCEPTION) {
        // Handle disconnected state like Python's SerialException catch block
        serial_interface_close(manager->serial_interface);

        return return_not_connected_data(manager, command);    
    }
    set_last_error(result);
    return result;
}

ErrorCode mode_manager_get_equipment_byte(ModeManager* manager, uint8_t* byte) {
    if (!manager || !byte) {
        set_last_error(ERROR_INVALID_PARAM);
        log_error("Invalid parameters in mode_manager_get_equipment_byte");
        return ERROR_INVALID_PARAM;
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
        mode_base_destroy(manager->active_mode);
        manager->active_mode = NULL;
    }

    if (manager->serial_interface) {
        serial_interface_close(manager->serial_interface);
    }

    if (manager->process_manager) {
        process_manager_destroy(manager->process_manager);  // Clean up our process_manager_instance (this doesn't destroy the instance created in main)         manager->process_manager = NULL;
    }

    log_debug("Mode manager closed successfully");
}
