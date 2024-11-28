#include <stdlib.h>
#include <string.h>
#include "mode_42.h"
#include "logger.h"

// VTable implementations
static int get_mode_number(const Mode* mode) {
    (void)mode;  // Unused parameter
    return 42;
}

static const uint8_t* get_emg_config_base(const Mode* mode, size_t* length) {
    Mode42Raw* raw_mode = (Mode42Raw*)mode->impl;
    *length = 1;
    
    if (raw_mode->filter_type == NOTCH_FILTER_NONE) {
        static const uint8_t config[] = {'r'};
        return config;
    }
    
    static uint8_t config[1];
    config[0] = (uint8_t)raw_mode->filter_type;
    return config;
}

static const ModeVTable mode_42_vtable = {
    .get_mode_number = get_mode_number,
    .get_emg_config = get_emg_config_base
};

// Equipment Byte Implementation
ErrorCode equipment_byte_create(EquipmentByte** mode, SerialInterface* interface) {
    if (!mode || !interface) {
        log_error("Invalid parameters in equipment_byte_create");
        return ERROR_INVALID_PARAMETER;
    }

    EquipmentByte* new_mode = (EquipmentByte*)malloc(sizeof(EquipmentByte));
    if (!new_mode) {
        log_error("Failed to allocate EquipmentByte");
        return ERROR_MEMORY_ALLOCATION;
    }

    ErrorCode error = mode_init(&new_mode->base, interface, &mode_42_vtable, new_mode);
    if (error != ERROR_NONE) {
        free(new_mode);
        return error;
    }

    new_mode->device_byte = 0;
    *mode = new_mode;
    log_debug("Equipment byte mode created successfully");
    return ERROR_NONE;
}

void equipment_byte_destroy(EquipmentByte* mode) {
    if (mode) {
        log_debug("Destroying Equipment byte mode");
        free(mode);
    }
}

static ErrorCode equipment_byte_execute(Mode* base, uint8_t* output, size_t* output_length) {
    EquipmentByte* mode = (EquipmentByte*)base->impl;
    
    ErrorCode error = serial_interface_handshake(mode->base.interface);
    if (error != ERROR_NONE) {
        return error;
    }

    output[0] = mode->device_byte;
    *output_length = 1;
    return ERROR_NONE;
}

static ErrorCode equipment_byte_execute_not_connected(Mode* base, uint8_t* output, size_t* output_length) {
    output[0] = 132;  // Default device byte for disconnected state
    *output_length = 1;
    return ERROR_NONE;
}

// Raw Mode Implementation
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

    while (remaining >= MODE_42_BLOCK_SIZE) {
        const uint8_t* next_block = values + MODE_42_BLOCK_SIZE;
        if (remaining >= 2 * MODE_42_BLOCK_SIZE) {
            bool stable = true;
            for (size_t i = 0; i < MODE_42_CMS_SIZE; i++) {
                if (abs((int)values[i] - (int)next_block[i]) >= 2) {
                    stable = false;
                    break;
                }
            }
            if (stable) {
                value_stable = true;
                break;
            }
        }
        values = next_block;
        remaining -= MODE_42_BLOCK_SIZE;
    }

    sync_result_free(&sync_result);
    return value_stable;
}

ErrorCode mode_42_raw_create(Mode42Raw** mode, SerialInterface* interface) {
    if (!mode || !interface) {
        log_error("Invalid parameters in mode_42_raw_create");
        return ERROR_INVALID_PARAMETER;
    }

    Mode42Raw* new_mode = (Mode42Raw*)malloc(sizeof(Mode42Raw));
    if (!new_mode) {
        log_error("Failed to allocate Mode42Raw");
        return ERROR_MEMORY_ALLOCATION;
    }

    ErrorCode error = mode_init(&new_mode->base, interface, &mode_42_vtable, new_mode);
    if (error != ERROR_NONE) {
        free(new_mode);
        return error;
    }

    new_mode->is_first_run = true;
    new_mode->filter_type = NOTCH_FILTER_NONE;
    *mode = new_mode;
    log_debug("Mode 42 Raw created successfully");
    return ERROR_NONE;
}

void mode_42_raw_destroy(Mode42Raw* mode) {
    if (mode) {
        log_debug("Destroying Mode 42 Raw");
        free(mode);
    }
}

static ErrorCode mode_42_raw_execute(Mode* base, uint8_t* output, size_t* output_length) {
    Mode42Raw* mode = (Mode42Raw*)base->impl;
    
    // Handle first run initialization
    if (mode->is_first_run) {
        uint8_t read_buffer[MODE_42_READ_SIZE];
        size_t bytes_read;
        int ignore_count = 0;
        size_t bytes_thrown = 0;

        while (bytes_thrown < MODE_42_INIT_THRESHOLD) {
            ErrorCode error = serial_interface_read(mode->base.interface, 
                                                  read_buffer, MODE_42_READ_SIZE, &bytes_read);
            if (error != ERROR_NONE) continue;

            bytes_thrown += bytes_read;
            
            if (wait_for_init(read_buffer, bytes_read)) {
                ignore_count++;
            }
            
            if (ignore_count > MODE_42_INIT_IGNORE_COUNT) break;
        }
        
        mode->is_first_run = false;
    }

    // Read and process data
    uint8_t read_buffer[1600];
    size_t bytes_read;
    
    ErrorCode error = serial_interface_read(mode->base.interface, read_buffer, 1600, &bytes_read);
    if (error != ERROR_NONE) {
        return error;
    }

    SyncResult sync_result;
    error = resync_bytes(read_buffer, bytes_read, MODE_42_BLOCK_SIZE,
                        sync_cms_channels, sync_emg_channels,
                        0, MODE_42_CMS_SIZE, &sync_result);

    if (!sync_result.found_sync) {
        log_error("Cannot verify byte order in Mode 42");
        sync_result_free(&sync_result);
        return ERROR_SYNC_FAILED;
    }

    size_t copy_size = sync_result.synced_length;
    if (copy_size > *output_length) {
        copy_size = *output_length;
    }
    
    memcpy(output, sync_result.synced_data, copy_size);
    *output_length = copy_size;

    sync_result_free(&sync_result);
    return ERROR_NONE;
}

static ErrorCode mode_42_raw_execute_not_connected(Mode* base, uint8_t* output, size_t* output_length) {
    static const uint8_t pattern[] = {
        7, 154, 23, 141, 40, 109, 55, 212, 
        136, 1, 152, 1, 168, 1, 184, 1, 
        200, 1, 216, 1, 232, 1, 248, 1
    };

    size_t pattern_size = sizeof(pattern);
    size_t max_repeats = *output_length / pattern_size;
    
    for (size_t i = 0; i < max_repeats; i++) {
        memcpy(output + (i * pattern_size), pattern, pattern_size);
    }
    
    *output_length = max_repeats * pattern_size;
    return ERROR_NONE;
}

// EMG Lead Status Implementation
ErrorCode emg_lead_status_create(GetEMGLeadStatus** mode, SerialInterface* interface) {
    if (!mode || !interface) {
        log_error("Invalid parameters in emg_lead_status_create");
        return ERROR_INVALID_PARAMETER;
    }

    GetEMGLeadStatus* new_mode = (GetEMGLeadStatus*)malloc(sizeof(GetEMGLeadStatus));
    if (!new_mode) {
        log_error("Failed to allocate GetEMGLeadStatus");
        return ERROR_MEMORY_ALLOCATION;
    }

    ErrorCode error = mode_init(&new_mode->base, interface, &mode_42_vtable, new_mode);
    if (error != ERROR_NONE) {
        free(new_mode);
        return error;
    }

    *mode = new_mode;
    log_debug("EMG Lead Status mode created successfully");
    return ERROR_NONE;
}

void emg_lead_status_destroy(GetEMGLeadStatus* mode) {
    if (mode) {
        log_debug("Destroying EMG Lead Status mode");
        free(mode);
    }
}

static ErrorCode emg_lead_status_execute(Mode* base, uint8_t* output, size_t* output_length) {
    GetEMGLeadStatus* mode = (GetEMGLeadStatus*)base->impl;
    
    ErrorCode error = serial_interface_handshake(mode->base.interface);
    if (error != ERROR_NONE) {
        return error;
    }

    uint8_t lead_byte;
    size_t bytes_read;
    
    error = serial_interface_read(mode->base.interface, &lead_byte, 1, &bytes_read);
    if (error != ERROR_NONE || bytes_read == 0) {
        lead_byte = 255;
    }

    output[0] = lead_byte;
    *output_length = 1;
    return ERROR_NONE;
}

static ErrorCode emg_lead_status_execute_not_connected(Mode* base, uint8_t* output, size_t* output_length) {
    output[0] = 255;  // Default lead status for disconnected state
    *output_length = 1;
    return ERROR_NONE;
}

// Notch Filter Variants
ErrorCode mode_42_raw_notch_create(Mode42Raw** mode, SerialInterface* interface, NotchFilterType filter_type) {
    ErrorCode error = mode_42_raw_create(mode, interface);
    if (error != ERROR_NONE) {
        return error;
    }

    (*mode)->filter_type = filter_type;
    log_debug("Mode 42 Raw Notch filter type %c created successfully", filter_type);
    return ERROR_NONE;
}