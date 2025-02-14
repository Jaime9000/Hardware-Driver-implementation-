#include <stdlib.h>
#include <string.h>
#include "mode_42.h"
#include "logger.h"
#include "simulation_function_generator_600mhz.h"

// Common functions
static int get_mode_number(const ModeBase* mode) {
    (void)mode;
    return 42;
}

static const uint8_t* get_emg_config(const ModeBase* mode, size_t* length) {
    Mode42* impl = (Mode42*)mode->impl;
    *length = 1;
    
    static uint8_t config[1];
    switch(impl->mode_type) {
        case MODE_42_TYPE_RAW_EMG:
            config[0] = 't';
            break;
        case MODE_42_TYPE_NOTCH_P:
            config[0] = 'p';
            break;
        case MODE_42_TYPE_NOTCH_Q:
            config[0] = 'q';
            break;
        case MODE_42_TYPE_NOTCH_R:
            config[0] = 'r';
            break;
        case MODE_42_TYPE_NOTCH_S:
            config[0] = 's';
            break;
        case MODE_42_TYPE_NOTCH_T:
            config[0] = 't';
            break;
        case MODE_42_TYPE_NOTCH_U:
            config[0] = 'u';
            break;
        case MODE_42_TYPE_NOTCH_V:
            config[0] = 'v';
            break;
        case MODE_42_TYPE_NOTCH_W:
            config[0] = 'w';
            break;
        default:
            config[0] = 'r';
            break;
    }
    return config;
}

static bool wait_for_init(const uint8_t* data, size_t length) {
    if (length < MODE_42_BLOCK_SIZE) return false;

    SyncResult sync_result;
    ErrorCode error = resync_bytes(data, length, MODE_42_BLOCK_SIZE,
                                 sync_cms_channels, sync_emg_channels,
                                 0, MODE_42_CMS_SIZE, &sync_result);
    
    if (!sync_result.found_sync) {
        sync_result_free(&sync_result);
        return false;
    }

    bool value_stable = false;
    const uint8_t* values = sync_result.synced_data;
    size_t remaining = sync_result.synced_length;

    // Process in 24-byte blocks, but only compare first 8 bytes (CMS data)
    while (remaining >= 2 * MODE_42_BLOCK_SIZE) {  // Need at least 2 blocks to compare
        // Get max difference between consecutive CMS blocks
        int max_diff = 0;
        for (size_t i = 0; i < MODE_42_CMS_SIZE; i++) {
            int diff = abs((int)values[i] - (int)(values[i + MODE_42_BLOCK_SIZE]));
            if (diff > max_diff) {
                max_diff = diff;
            }
        }
        
        // Check if all differences are less than 2 (matching Python's max(diff) < 2)
        if (max_diff < 2) {
            value_stable = true;
            break;
        }
        
        values += MODE_42_BLOCK_SIZE;
        remaining -= MODE_42_BLOCK_SIZE;
    }

    sync_result_free(&sync_result);
    return value_stable;
}

// Execute mode implementations
static ErrorCode execute_mode_raw(ModeBase* base, uint8_t* output, size_t* output_length) {
    Mode42* impl = (Mode42*)base->impl;
    
    if (impl->is_first_run) {
        // Initialize EMG configuration
        size_t config_length;
        const uint8_t* config = get_emg_config(base, &config_length);
        
        ErrorCode error = serial_interface_write(base->interface, config, config_length);
        if (error != ERROR_NONE) {
            return error;
        }
        
        // Initialize and wait for stable data
        uint8_t read_buffer[MODE_42_READ_SIZE];
        size_t bytes_read;
        int ignore_count = 0;
        size_t bytes_thrown = 0;

        while (bytes_thrown < MODE_42_INIT_THRESHOLD) {
            error = serial_interface_read(base->interface, read_buffer, MODE_42_READ_SIZE, &bytes_read);
            if (error != ERROR_NONE) continue;

            bytes_thrown += bytes_read;
            
            if (wait_for_init(read_buffer, bytes_read)) {
                ignore_count++;
            }
            
            if (ignore_count > MODE_42_INIT_IGNORE_COUNT) break;
        }
        
        impl->is_first_run = false;
    }
    
    size_t bytes_to_read = MODE_42_READ_SIZE;
    return serial_interface_read(base->interface, output, &bytes_to_read, MODE_42_TIMEOUT_MS);
}

static ErrorCode execute_mode_equipment(ModeBase* base, uint8_t* output, size_t* output_length) {
    EquipmentByte* mode = (EquipmentByte*)base->impl;
    
    ErrorCode error = mode_base_handshake(base);
    if (error != ERROR_NONE) {
        return error;
    }

    output[0] = mode->device_byte;
    *output_length = 1;
    return ERROR_NONE;
}

static ErrorCode execute_mode_lead_status(ModeBase* base, uint8_t* output, size_t* output_length) {
    GetEMGLeadStatus* mode = (GetEMGLeadStatus*)base->impl;
    
    ErrorCode error = mode_base_handshake(base);
    if (error != ERROR_NONE) {
        return error;
    }

    uint8_t lead_byte;
    size_t bytes_read = 1;
    
    error = serial_interface_read(base->interface, &lead_byte, &bytes_read, MODE_42_TIMEOUT_MS);
    if (error != ERROR_NONE || bytes_read == 0) {
        lead_byte = 255;
    }

    output[0] = lead_byte;
    *output_length = 1;
    return ERROR_NONE;
}

// Not connected mode implementations
static ErrorCode execute_mode_not_connected_raw(ModeBase* base, uint8_t* output, size_t* output_length) {
    static const uint8_t pattern[] = {
        7, 154, 23, 141, 40, 109, 55, 212, 
        136, 1, 152, 1, 168, 1, 184, 1, 
        200, 1, 216, 1, 232, 1, 248, 1
    };
    
    // Calculate how many complete patterns we can fit (400 times like Python)
    const size_t pattern_size = sizeof(pattern);
    const size_t desired_repeats = 400;
    const size_t max_repeats = *output_length / pattern_size;
    const size_t repeats = (max_repeats < desired_repeats) ? max_repeats : desired_repeats;
    
    for (size_t i = 0; i < repeats; i++) {
        memcpy(output + (i * pattern_size), pattern, pattern_size);
    }
    
    *output_length = repeats * pattern_size;
    return ERROR_NONE;
}

static ErrorCode execute_mode_not_connected_equipment(ModeBase* base, uint8_t* output, size_t* output_length) {
    output[0] = 132;  // Default equipment byte for disconnected state
    *output_length = 1;
    return ERROR_NONE;
}

static ErrorCode execute_mode_not_connected_lead_status(ModeBase* base, uint8_t* output, size_t* output_length) {
    output[0] = 255;  // Default lead status for disconnected state
    *output_length = 1;
    return ERROR_NONE;
}

// VTable definitions
static const ModeBaseVTable mode_42_raw_vtable = {
    .get_mode_number = get_mode_number,
    .get_emg_config = get_emg_config,
    .execute_mode = execute_mode_raw,
    .execute_mode_not_connected = execute_mode_not_connected_raw,
    .stop = NULL,
    .destroy = NULL
};

static const ModeBaseVTable mode_42_equipment_vtable = {
    .get_mode_number = get_mode_number,
    .get_emg_config = get_emg_config,
    .execute_mode = execute_mode_equipment,
    .execute_mode_not_connected = execute_mode_not_connected_equipment,
    .stop = NULL,
    .destroy = NULL
};

static const ModeBaseVTable mode_42_lead_status_vtable = {
    .get_mode_number = get_mode_number,
    .get_emg_config = get_emg_config,
    .execute_mode = execute_mode_lead_status,
    .execute_mode_not_connected = execute_mode_not_connected_lead_status,
    .stop = NULL,
    .destroy = NULL
};

// Constructor implementations
static ErrorCode mode_42_create_base(Mode42** mode, SerialInterface*serial_interface, 
                                   ProcessManager* process_manager, Mode42Type type) {
    if (!mode || !interface || !process_manager) {
        return ERROR_INVALID_PARAM;
    }

    Mode42* new_mode = (Mode42*)malloc(sizeof(Mode42));
    if (!new_mode) {
        return ERROR_MEMORY_ALLOCATION;
    }

    ErrorCode error = mode_base_create(&new_mode->base,serial_interface, process_manager, 
                                     &mode_42_raw_vtable, new_mode);
    if (error != ERROR_NONE) {
        free(new_mode);
        return error;
    }

    new_mode->is_first_run = true;
    new_mode->mode_type = type;

    *mode = new_mode;
    return ERROR_NONE;
}

ErrorCode mode_42_raw_create(Mode42** mode, SerialInterface*serial_interface, ProcessManager* process_manager) {
    return mode_42_create_base(mode,serial_interface, process_manager, MODE_42_TYPE_RAW);
}

ErrorCode mode_42_raw_emg_create(Mode42** mode, SerialInterface*serial_interface, ProcessManager* process_manager) {
    return mode_42_create_base(mode,serial_interface, process_manager, MODE_42_TYPE_RAW_EMG);
}

ErrorCode mode_42_equipment_create(EquipmentByte** mode, SerialInterface*serial_interface, ProcessManager* process_manager) {
    if (!mode || !interface || !process_manager) {
        log_error("Invalid parameters in mode_42_equipment_create");
        return ERROR_INVALID_PARAM;
    }

    EquipmentByte* new_mode = (EquipmentByte*)malloc(sizeof(EquipmentByte));
    if (!new_mode) {
        log_error("Failed to allocate EquipmentByte");
        return ERROR_MEMORY_ALLOCATION;
    }

    ErrorCode error = mode_base_create(&new_mode->base,serial_interface, process_manager, 
                                     &mode_42_equipment_vtable, new_mode);
    if (error != ERROR_NONE) {
        free(new_mode);
        return error;
    }

    new_mode->device_byte = 0;
    *mode = new_mode;
    log_debug("Equipment byte mode created successfully");
    return ERROR_NONE;
}

ErrorCode mode_42_lead_status_create(GetEMGLeadStatus** mode, SerialInterface*serial_interface, ProcessManager* process_manager) {
    if (!mode || !interface || !process_manager) {
        log_error("Invalid parameters in emg_lead_status_create");
        return ERROR_INVALID_PARAM;
    }

    GetEMGLeadStatus* new_mode = (GetEMGLeadStatus*)malloc(sizeof(GetEMGLeadStatus));
    if (!new_mode) {
        log_error("Failed to allocate GetEMGLeadStatus");
        return ERROR_MEMORY_ALLOCATION;
    }

    // Initialize Mode43 base first
    ErrorCode error = mode_43_raw_create((Mode43**)&new_mode->base43,serial_interface, process_manager);
    if (error != ERROR_NONE) {
        free(new_mode);
        return error;
    }

    // Initialize Mode42 base with lead status vtable
    error = mode_base_create(&new_mode->mode42_base,serial_interface, process_manager, 
                           &mode_42_lead_status_vtable, new_mode);
    if (error != ERROR_NONE) {
        mode_43_destroy(&new_mode->base43);
        free(new_mode);
        return error;
    }

    *mode = new_mode;
    log_debug("EMG Lead Status mode created successfully");
    return ERROR_NONE;
}

ErrorCode mode_42_raw_notch_create(Mode42** mode, SerialInterface*serial_interface, 
                                 ProcessManager* process_manager, Mode42Type notch_type) {
    if (notch_type < MODE_42_TYPE_NOTCH_P || notch_type > MODE_42_TYPE_NOTCH_W) {
        return ERROR_INVALID_PARAM;
    }
    return mode_42_create_base(mode,serial_interface, process_manager, notch_type);
}

// Destructor implementations
void mode_42_destroy(Mode42* mode) {
    if (mode) {
        mode_base_destroy(&mode->base);
        free(mode);
    }
}

void equipment_byte_destroy(EquipmentByte* mode) {
    if (mode) {
        mode_base_destroy(&mode->base);
        free(mode);
    }
}

void emg_lead_status_destroy(GetEMGLeadStatus* mode) {
    if (mode) {
        mode_43_destroy(&mode->base43);
        mode_base_destroy(mode->mode42_base);
        free(mode->mode42_base);
        free(mode);
        log_debug("EMG Lead Status mode destroyed");
    }
}