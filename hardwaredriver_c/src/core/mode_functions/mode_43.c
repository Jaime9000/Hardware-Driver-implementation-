#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "mode_43.h"
#include "logger.h"
#include "simulation_function_generator_600mhz.h"

static int get_mode_number(const ModeBase* mode) {
    (void)mode;
    return 43;
}

static const uint8_t* get_emg_config(const ModeBase* mode, size_t* length) {
    Mode43* impl = (Mode43*)mode->impl;
    *length = 1;
    
    static uint8_t config[1];
    switch(impl->mode_type) {
        case MODE_43_TYPE_RAW_EMG:
            config[0] = 't';
            break;
        case MODE_43_TYPE_NOTCH_P:
            config[0] = 'p';
            break;
        case MODE_43_TYPE_NOTCH_Q:
            config[0] = 'q';
            break;
        case MODE_43_TYPE_NOTCH_R:
            config[0] = 'r';
            break;
        case MODE_43_TYPE_NOTCH_S:
            config[0] = 's';
            break;
        case MODE_43_TYPE_NOTCH_T:
            config[0] = 't';
            break;
        case MODE_43_TYPE_NOTCH_U:
            config[0] = 'u';
            break;
        case MODE_43_TYPE_NOTCH_V:
            config[0] = 'v';
            break;
        case MODE_43_TYPE_NOTCH_W:
            config[0] = 'w';
            break;
        default:
            config[0] = 'r';
            break;
    }
    return config;
}

static ErrorCode execute_mode_raw_emg(ModeBase* base, uint8_t* output, size_t* output_length) {
    Mode43* impl = (Mode43*)base->impl;
    
    if (impl->is_first_run) {
        // Skip initial bytes on first run
        uint8_t buffer[MODE_43_INIT_BYTES];
        size_t bytes_read = MODE_43_INIT_BYTES;
        
        ErrorCode error = serial_interface_read(base->interface, buffer, &bytes_read, MODE_43_TIMEOUT_MS);
        if (error != ERROR_NONE) {
            return error;
        }
        
        impl->is_first_run = false;
    }
    
    size_t bytes_to_read = MODE_43_READ_SIZE;
    return serial_interface_read(base->interface, output, &bytes_to_read, MODE_43_TIMEOUT_MS);
}

static ErrorCode execute_mode_raw(ModeBase* base, uint8_t* output, size_t* output_length) {
    Mode43* impl = (Mode43*)base->impl;
    
    if (impl->is_first_run) {
        // Initialize EMG configuration
        size_t config_length;
        const uint8_t* config = get_emg_config(base, &config_length);
        
        ErrorCode error = serial_interface_write(base->interface, config, config_length);
        if (error != ERROR_NONE) {
            return error;
        }
        
        // Skip initial bytes
        uint8_t buffer[MODE_43_INIT_BYTES];
        size_t bytes_read = MODE_43_INIT_BYTES;
        
        error = serial_interface_read(base->interface, buffer, &bytes_read, MODE_43_TIMEOUT_MS);
        if (error != ERROR_NONE) {
            return error;
        }
        
        impl->is_first_run = false;
    }
    
    size_t bytes_to_read = MODE_43_READ_SIZE;
    return serial_interface_read(base->interface, output, &bytes_to_read, MODE_43_TIMEOUT_MS);
}

static ErrorCode execute_mode_not_connected(ModeBase* base, uint8_t* output, size_t* output_length) {
    Mode43* impl = (Mode43*)base->impl;
    
    // Calculate total size needed (all samples flattened)
    size_t total_size = SIMULATION_SAMPLE_COUNT * SIMULATION_SAMPLE_WIDTH;
    
    // Check if output buffer is large enough
    if (total_size > *output_length) {
        return ERROR_BUFF_OVERFLOW;
    }

    // Flatten the 2D array into the output buffer
    size_t offset = 0;
    for (size_t i = 0; i < SIMULATION_SAMPLE_COUNT; i++) {
        const uint8_t* sample = get_simulation_sample_data(i);
        if (!sample) {
            return ERROR_INVALID_STATE;
        }
        
        memcpy(output + offset, sample, SIMULATION_SAMPLE_WIDTH);
        offset += SIMULATION_SAMPLE_WIDTH;
    }

    *output_length = total_size;
    return ERROR_NONE;
}

static const ModeVTable mode_43_vtable = {
    .get_mode_number = get_mode_number,
    .execute_mode = execute_mode_raw,
    .execute_mode_raw_emg = execute_mode_raw_emg,
    .execute_mode_not_connected = execute_mode_not_connected,
    .stop = NULL,
    .destroy = NULL
};

static ErrorCode mode_43_create_base(Mode43** mode, SerialInterface* interface, 
                                   ProcessManager* process_manager, Mode43Type type) {
    if (!mode || !interface || !process_manager) {
        return ERROR_INVALID_PARAM;
    }

    Mode43* new_mode = (Mode43*)malloc(sizeof(Mode43));
    if (!new_mode) {
        return ERROR_MEMORY_ALLOCATION;
    }

    ErrorCode error = mode_base_create(&new_mode->base, interface, process_manager, 
                                     &mode_43_vtable, new_mode);
    if (error != ERROR_NONE) {
        free(new_mode);
        return error;
    }

    new_mode->is_first_run = true;
    new_mode->mode_type = type;

    *mode = new_mode;
    return ERROR_NONE;
}

ErrorCode mode_43_raw_create(Mode43** mode, SerialInterface* interface, ProcessManager* process_manager) {
    return mode_43_create_base(mode, interface, process_manager, MODE_43_TYPE_RAW);
}

ErrorCode mode_43_raw_emg_create(Mode43** mode, SerialInterface* interface, ProcessManager* process_manager) {
    return mode_43_create_base(mode, interface, process_manager, MODE_43_TYPE_RAW_EMG);
}

ErrorCode mode_43_raw_notch_create(Mode43** mode, SerialInterface* interface, 
                                 ProcessManager* process_manager, Mode43Type notch_type) {
    if (notch_type < MODE_43_TYPE_NOTCH_P || notch_type > MODE_43_TYPE_NOTCH_W) {
        return ERROR_INVALID_PARAM;
    }
    return mode_43_create_base(mode, interface, process_manager, notch_type);
}

void mode_43_destroy(Mode43* mode) {
    if (mode) {
        mode_base_destroy(&mode->base);
        free(mode);
    }
}