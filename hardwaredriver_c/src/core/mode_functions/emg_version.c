#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "emg_version.h"
#include "logger.h"

// VTable implementations
static int get_mode_number(const Mode* mode) {
    (void)mode;  // Unused parameter
    return 118;
}

static const uint8_t* get_emg_config(const Mode* mode, size_t* length) {
    static const uint8_t config[] = {'r'};
    *length = sizeof(config);
    return config;
}

static const ModeVTable emg_version_vtable = {
    .get_mode_number = get_mode_number,
    .get_emg_config = get_emg_config
};

// EMGVersionAdsOn Implementation
ErrorCode emg_version_create(EMGVersionAdsOn** mode, SerialInterface* interface) {
    if (!mode || !interface) {
        log_error("Invalid parameters in emg_version_create");
        return ERROR_INVALID_PARAMETER;
    }

    EMGVersionAdsOn* new_mode = (EMGVersionAdsOn*)malloc(sizeof(EMGVersionAdsOn));
    if (!new_mode) {
        log_error("Failed to allocate EMGVersionAdsOn");
        return ERROR_MEMORY_ALLOCATION;
    }

    ErrorCode error = mode_init(&new_mode->base, interface, &emg_version_vtable, new_mode);
    if (error != ERROR_NONE) {
        free(new_mode);
        return error;
    }

    memset(new_mode->version_string, 0, VERSION_STRING_MAX_LENGTH);
    *mode = new_mode;
    log_debug("EMG Version mode created successfully");
    return ERROR_NONE;
}

void emg_version_destroy(EMGVersionAdsOn* mode) {
    if (mode) {
        log_debug("Destroying EMG Version mode");
        free(mode);
    }
}

static ErrorCode emg_version_execute(Mode* base, uint8_t* output, size_t* output_length) {
    EMGVersionAdsOn* mode = (EMGVersionAdsOn*)base->impl;
    
    ErrorCode error = serial_interface_handshake(mode->base.interface);
    if (error != ERROR_NONE) {
        return error;
    }

    uint8_t version_data[VERSION_DATA_LENGTH];
    size_t bytes_read;
    
    error = serial_interface_read(mode->base.interface, version_data, VERSION_DATA_LENGTH, &bytes_read);
    if (error != ERROR_NONE || bytes_read != VERSION_DATA_LENGTH) {
        log_error("Unable to parse all values");
        return ERROR_INVALID_DATA;
    }

    // Format version string based on data
    if (version_data[2] == 0 && version_data[3] == 0) {
        strncpy(mode->version_string, "0.0", VERSION_STRING_MAX_LENGTH - 1);
    } else {
        snprintf(mode->version_string, VERSION_STRING_MAX_LENGTH, "%d.%d", 
                version_data[2] - 48, version_data[3] - 48);
    }

    size_t version_length = strlen(mode->version_string);
    if (*output_length < version_length) {
        log_error("Output buffer too small for version string");
        return ERROR_BUFFER_OVERFLOW;
    }

    memcpy(output, mode->version_string, version_length);
    *output_length = version_length;
    
    return ERROR_NONE;
}

static ErrorCode emg_version_execute_not_connected(Mode* base, uint8_t* output, size_t* output_length) {
    const char* default_version = "1.2";
    size_t version_length = strlen(default_version);
    
    if (*output_length < version_length) {
        log_error("Output buffer too small for version string");
        return ERROR_BUFFER_OVERFLOW;
    }

    memcpy(output, default_version, version_length);
    *output_length = version_length;
    return ERROR_NONE;
}

// Hardware Connection Check Implementation
ErrorCode hardware_connection_create(CheckHardwareConnection** mode, SerialInterface* interface) {
    if (!mode || !interface) {
        log_error("Invalid parameters in hardware_connection_create");
        return ERROR_INVALID_PARAMETER;
    }

    CheckHardwareConnection* new_mode = (CheckHardwareConnection*)malloc(sizeof(CheckHardwareConnection));
    if (!new_mode) {
        log_error("Failed to allocate CheckHardwareConnection");
        return ERROR_MEMORY_ALLOCATION;
    }

    ErrorCode error = emg_version_create(&new_mode->base, interface);
    if (error != ERROR_NONE) {
        free(new_mode);
        return error;
    }

    *mode = new_mode;
    log_debug("Hardware Connection Check mode created successfully");
    return ERROR_NONE;
}

void hardware_connection_destroy(CheckHardwareConnection* mode) {
    if (mode) {
        log_debug("Destroying Hardware Connection Check mode");
        emg_version_destroy(&mode->base);
        free(mode);
    }
}

static ErrorCode hardware_connection_execute(Mode* base, uint8_t* output, size_t* output_length) {
    // Call parent's execute method
    return emg_version_execute(base, output, output_length);
}

static ErrorCode hardware_connection_execute_not_connected(Mode* base, uint8_t* output, size_t* output_length) {
    const char* not_connected = "not-connected";
    size_t msg_length = strlen(not_connected);
    
    if (*output_length < msg_length) {
        log_error("Output buffer too small for not-connected message");
        return ERROR_BUFFER_OVERFLOW;
    }

    memcpy(output, not_connected, msg_length);
    *output_length = msg_length;
    return ERROR_NONE;
}