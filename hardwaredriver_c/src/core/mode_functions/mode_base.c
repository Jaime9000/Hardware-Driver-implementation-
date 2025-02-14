#include "src/core/mode_functions/mode_base.h"
#include "src/core/mode_functions/timing.h"
#include "src/core/logger.h"
#include <stdlib.h>
#include <string.h>

#define MAX_FLUSH_BYTES 2000
#define FLUSH_TIMEOUT_MS 80
#define READ_CHUNK_SIZE 320

ErrorCode mode_base_create(ModeBase** mode, SerialInterface*serial_interface, 
                         ProcessManager* process_manager, 
                         const ModeBaseVTable* vtable, void* impl) {
    if (!mode || !interface || !process_manager || !vtable || !impl) {
        log_error("Invalid parameters in mode_base_create");
        return ERROR_INVALID_PARAM;
    }

    ModeBase* new_mode = (ModeBase*)calloc(1, sizeof(ModeBase));
    if (!new_mode) {
        log_error("Failed to allocate ModeBase");
        return ERROR_MEMORY_ALLOCATION;
    }

    new_mode->interface =serial_interface;
    new_mode->process_manager = process_manager;
    new_mode->vtable = vtable;
    new_mode->impl = impl;
    new_mode->handshake_established = false;
    new_mode->device_byte = 0;

    *mode = new_mode;
    log_debug("ModeBase created successfully");
    return ERROR_NONE;
}

void mode_base_destroy(ModeBase* mode) {
    if (!mode) return;

    if (mode->vtable && mode->vtable->destroy) {
        mode->vtable->destroy(mode);
    }
    
    if (mode->vtable && mode->vtable->stop) {
        mode->vtable->stop(mode);
    }

    free(mode);
}

ErrorCode mode_base_handshake(ModeBase* mode) {
    if (!mode || !mode->interface) {
        return ERROR_INVALID_PARAM;
    }

    // Reset hardware at 60Hz
    ErrorCode result = serial_interface_reset_hardware(mode->interface, true);
    if (result != ERROR_NONE) {
        log_error("Failed to reset hardware during handshake");
        return result;
    }

    // Write mode number
    int mode_num = mode_base_get_number(mode);
    if (mode_num < 0) {
        log_error("Invalid mode number during handshake");
        return ERROR_INVALID_MODE;
    }

    uint8_t mode_byte = (uint8_t)mode_num;
    result = serial_interface_write_data(mode->interface, &mode_byte, 1);
    if (result != ERROR_NONE) {
        log_error("Failed to write mode byte");
        return result;
    }

    // Write EMG config if available
    size_t config_len;
    const uint8_t* config = mode_base_get_emg_config(mode, &config_len);
    if (config && config_len > 0) {
        result = serial_interface_write_data(mode->interface, config, config_len);
        if (result != ERROR_NONE) {
            log_error("Failed to write EMG config");
            return result;
        }
    }

    // Read device byte
    uint8_t device_byte;
    size_t bytes_read;
    result = serial_interface_read_data(mode->interface, &device_byte, &bytes_read, 1);
    
    if (result != ERROR_NONE || bytes_read != 1) {
        log_error("Failed to read device byte");
        return (result != ERROR_NONE) ? result : ERROR_NO_DATA;
    }

    mode->device_byte = device_byte;
    mode->handshake_established = true;
    log_info("Handshake completed successfully, device byte: %d", device_byte);
    
    return ERROR_NONE;
}

ErrorCode mode_base_execute(ModeBase* mode, uint8_t* output, size_t* output_length, bool disconnected) {
    if (!mode || !output || !output_length) {
        return ERROR_INVALID_PARAM;
    }

    *output_length = 0;

    if (disconnected) {
        if (!mode->vtable->execute_mode_not_connected) {
            return ERROR_NOT_IMPLEMENTED;
        }
        return mode->vtable->execute_mode_not_connected(mode, output, output_length);
    }

    if (!mode->handshake_established) {
        ErrorCode result = mode_base_handshake(mode);
        if (result != ERROR_NONE) {
            return result;
        }
    }

    if (!mode->vtable->execute_mode) {
        return ERROR_NOT_IMPLEMENTED;
    }

    return mode->vtable->execute_mode(mode, output, output_length);
}

ErrorCode mode_base_flush_data(ModeBase* mode) {
    if (!mode || !mode->interface) {
        return ERROR_INVALID_PARAM;
    }

    uint8_t buffer[READ_CHUNK_SIZE];
    size_t total_bytes = 0;
    uint64_t start_time = get_current_time_ms();

    while (total_bytes < MAX_FLUSH_BYTES) {
        size_t bytes_read;
        ErrorCode result = serial_interface_read_data(mode->interface, buffer, &bytes_read, READ_CHUNK_SIZE);
        
        if (result != ERROR_NONE && result != ERROR_TIME_OUT) {
            return result;
        }

        total_bytes += bytes_read;

        if (get_current_time_ms() - start_time > FLUSH_TIMEOUT_MS) {
            break;
        }
    }

    return ERROR_NONE;
}

void mode_base_stop(ModeBase* mode) {
    if (!mode || !mode->vtable || !mode->vtable->stop) {
        return;
    }
    mode->vtable->stop(mode);
}

// Getter implementations
int mode_base_get_number(const ModeBase* mode) {
    if (!mode || !mode->vtable || !mode->vtable->get_mode_number) {
        return -1;
    }
    return mode->vtable->get_mode_number(mode);
}

const uint8_t* mode_base_get_emg_config(const ModeBase* mode, size_t* length) {
    if (!mode || !mode->vtable || !mode->vtable->get_emg_config || !length) {
        if (length) *length = 0;
        return NULL;
    }
    return mode->vtable->get_emg_config(mode, length);
}

uint8_t mode_base_get_device_byte(const ModeBase* mode) {
    return mode ? mode->device_byte : 0;
}

bool mode_base_is_handshake_established(const ModeBase* mode) {
    return mode ? mode->handshake_established : false;
}

bool mode_base_is_sweep(const ModeBase* mode) {
    if (!mode || !mode->vtable) {
        return false;
    }
    
    int mode_num = mode->vtable->get_mode_number(mode);
    return (mode_num == MODE_44_SWEEP || 
            mode_num == MODE_57_SWEEP ||
            mode_num == MODE_44_RAW_SWEEP_NO_IMAGE);
}