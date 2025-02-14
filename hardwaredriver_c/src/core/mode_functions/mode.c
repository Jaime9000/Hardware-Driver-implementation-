#include "src/core/mode_functions/mode.h"
#include "src/core/logger.h"
#include "src/core/mode_functions/timing.h"

#include <string.h>

#define MAX_FLUSH_BYTES 2000
#define FLUSH_TIMEOUT_MS 80
#define READ_CHUNK_SIZE 320

ErrorCode mode_init(Mode* mode, SerialInterface*serial_interface, const ModeVTable* vtable, void* impl) {
    if (!mode || !interface || !vtable || !impl) {
        set_last_error(ERROR_INVALID_PARAM);
        log_error("Invalid parameters in mode_init");
        return ERROR_INVALID_PARAM;
    }

    mode->interface =serial_interface;
    mode->vtable = vtable;
    mode->impl = impl;
    mode->handshake_established = false;
    mode->device_byte = 0;

    log_debug("Mode initialized successfully");
    set_last_error(ERROR_NONE);
    return ERROR_NONE;
}

void mode_destroy(Mode* mode) {
    if (!mode) {
        log_warning("Attempted to destroy NULL mode");
        return;
    }

    if (mode->vtable && mode->vtable->destroy) {
        mode->vtable->destroy(mode);
        log_debug("Mode destroyed successfully");
    } else {
        log_warning("Mode destroy function not implemented");
    }
}

ErrorCode mode_handshake(Mode* mode) {
    if (!mode || !mode->interface) {
        set_last_error(ERROR_INVALID_PARAM);
        log_error("Invalid parameters in mode_handshake");
        return ERROR_INVALID_PARAM;
    }

    // Reset hardware at 60Hz
    log_debug("Initiating hardware reset at 60Hz");
    ErrorCode result = serial_interface_reset_hardware(mode->interface, true);
    if (result != ERROR_NONE) {
        set_last_error(result);
        log_error("Failed to reset hardware: %s", get_error_string(result));
        return result;
    }

    // Get mode number and write it
    int mode_num = mode_get_number(mode);
    uint8_t mode_byte = (uint8_t)mode_num;
    log_debug("Writing mode byte: %d", mode_byte);
    result = serial_interface_write(mode->interface, &mode_byte, 1);
    if (result != ERROR_NONE) {
        set_last_error(result);
        log_error("Failed to write mode byte: %s", get_error_string(result));
        return result;
    }

    // Write EMG config
    size_t config_len;
    const uint8_t* config = mode_get_emg_config(mode, &config_len);
    if (config && config_len > 0) {
        log_debug("Writing EMG config, length: %zu", config_len);
        result = serial_interface_write(mode->interface, config, config_len);
        if (result != ERROR_NONE) {
            set_last_error(result);
            log_error("Failed to write EMG config: %s", get_error_string(result));
            return result;
        }
    }

    // Read device byte
    uint8_t device_byte;
    size_t bytes_read;
    log_debug("Reading device byte");
    result = serial_interface_read(mode->interface, &device_byte, 1, &bytes_read);
    
    if (result != ERROR_NONE) {
        set_last_error(result);
        log_error("Failed to read device byte: %s", get_error_string(result));
        return result;
    }

    if (bytes_read == 1) {
        log_info("Device byte received: %d", device_byte);
        mode->device_byte = device_byte;
        mode->handshake_established = true;
    } else {
        set_last_error(ERROR_NO_DATA);
        log_error("No device byte received");
        return ERROR_NO_DATA;
    }

    log_debug("Handshake completed successfully");
    set_last_error(ERROR_NONE);
    return ERROR_NONE;
}

ErrorCode mode_execute(Mode* mode, bool disconnected) {
    if (!mode || !mode->vtable) {
        set_last_error(ERROR_INVALID_PARAM);
        log_error("Invalid parameters in mode_execute");
        return ERROR_INVALID_PARAM;
    }

    if (disconnected) {
        log_debug("Executing in disconnected mode");
        if (!mode->vtable->execute_not_connected) {
            set_last_error(ERROR_NOT_IMPLEMENTED);
            log_error("Execute not connected not implemented");
            return ERROR_NOT_IMPLEMENTED;
        }
        ErrorCode result = mode->vtable->execute_not_connected(mode);
        set_last_error(result);
        return result;
    }

    if (!mode->handshake_established) {
        log_debug("Handshake not established, performing handshake");
        ErrorCode result = mode_handshake(mode);
        if (result != ERROR_NONE) {
            set_last_error(result);
            log_error("Handshake failed: %s", get_error_string(result));
            return result;
        }
    }

    if (!mode->vtable->execute) {
        set_last_error(ERROR_NOT_IMPLEMENTED);
        log_error("Execute function not implemented");
        return ERROR_NOT_IMPLEMENTED;
    }

    log_debug("Executing mode");
    ErrorCode result = mode->vtable->execute(mode);
    set_last_error(result);
    return result;
}

ErrorCode mode_flush_data(Mode* mode) {
    if (!mode || !mode->interface) {
        set_last_error(ERROR_INVALID_PARAM);
        log_error("Invalid parameters in mode_flush_data");
        return ERROR_INVALID_PARAM;
    }

    uint8_t buffer[READ_CHUNK_SIZE];
    size_t total_bytes = 0;
    uint64_t start_time = get_current_time_ms();

    log_debug("Starting data flush");
    while (total_bytes < MAX_FLUSH_BYTES) {
        size_t bytes_read;
        ErrorCode result = serial_interface_read(mode->interface, buffer, READ_CHUNK_SIZE, &bytes_read);
        
        if (result != ERROR_NONE && result != ERROR_TIME_OUT) {
            set_last_error(result);
            log_error("Flush failed: %s", get_error_string(result));
            return result;
        }

        total_bytes += bytes_read;

        uint64_t elapsed_ms = get_current_time_ms() - start_time;
        if (elapsed_ms > FLUSH_TIMEOUT_MS) {
            log_debug("Flush timeout reached after %zu bytes", total_bytes);
            break;
        }
    }

    log_debug("Flush completed, total bytes: %zu", total_bytes);
    set_last_error(ERROR_NONE);
    return ERROR_NONE;
}

int mode_get_number(const Mode* mode) {
    if (!mode || !mode->vtable || !mode->vtable->get_mode_number) {
        log_error("Cannot get mode number: invalid mode or vtable");
        return -1;
    }
    return mode->vtable->get_mode_number(mode);
}

const uint8_t* mode_get_emg_config(const Mode* mode, size_t* length) {
    if (!mode || !mode->vtable || !mode->vtable->get_emg_config || !length) {
        log_error("Cannot get EMG config: invalid parameters");
        if (length) {
            *length = 0;
        }
        return NULL;
    }
    return mode->vtable->get_emg_config(mode, length);
}

uint8_t mode_get_device_byte(const Mode* mode) {
    if (!mode) {
        log_warning("Attempted to get device byte from NULL mode");
        return 0;
    }
    return mode->device_byte;
}

bool mode_is_handshake_established(const Mode* mode) {
    if (!mode) {
        log_warning("Attempted to check handshake status of NULL mode");
        return false;
    }
    return mode->handshake_established;
}