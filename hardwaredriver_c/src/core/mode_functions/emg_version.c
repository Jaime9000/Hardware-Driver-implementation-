#include "src/core/mode_functions/emg_version.h"
#include "src/core/logger.h"

// System headers
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// VTable implementations for EMGVersionAdsOn
static int emg_version_get_number(const ModeBase* mode) {
    (void)mode;
    return 118;
}

static const uint8_t* emg_version_get_emg_config(const ModeBase* mode, size_t* length) {
    static const uint8_t config[] = {'r'};
    *length = sizeof(config);
    return config;
}

static ErrorCode emg_version_execute(ModeBase* mode, uint8_t* output, size_t* output_length) {
    EMGVersionImpl* impl = (EMGVersionImpl*)mode->impl;
    
    ErrorCode error = mode_base_handshake(mode);
    if (error != ERROR_NONE) {
        return error;
    }

    uint8_t version_data[VERSION_DATA_LENGTH];
    size_t bytes_read;
    
    error = serial_interface_read_data(mode->interface, version_data, &bytes_read, VERSION_DATA_LENGTH);
    if (error != ERROR_NONE || bytes_read != VERSION_DATA_LENGTH) {
        log_error("Unable to parse all values");
        return ERROR_INVALID_DATA;
    }

    // Format version string based on data
    if (version_data[2] == 0 && version_data[3] == 0) {
        strncpy(impl->version_string, "0.0", VERSION_STRING_MAX_LENGTH - 1);
    } else {
        snprintf(impl->version_string, VERSION_STRING_MAX_LENGTH, "%d.%d", 
                version_data[2] - 48, version_data[3] - 48);
    }

    size_t version_length = strlen(impl->version_string);
    if (*output_length < version_length) {
        log_error("Output buffer too small for version string");
        return ERROR_BUFFER_OVERFLOW;
    }

    memcpy(output, impl->version_string, version_length);
    *output_length = version_length;
    
    return ERROR_NONE;
}

static ErrorCode emg_version_execute_not_connected(ModeBase* mode, uint8_t* output, size_t* output_length) {
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

static void emg_version_stop(ModeBase* mode) {
    // No specific stop functionality needed
    (void)mode;
}

static void emg_version_destroy_impl(ModeBase* mode) {
    if (mode && mode->impl) {
        free(mode->impl);
        mode->impl = NULL;
    }
}

// VTable for EMGVersionAdsOn
static const ModeBaseVTable emg_version_vtable = {
    .get_mode_number = emg_version_get_number,
    .get_emg_config = emg_version_get_emg_config,
    .execute_mode = emg_version_execute,
    .execute_mode_not_connected = emg_version_execute_not_connected,
    .stop = emg_version_stop,
    .destroy = emg_version_destroy_impl
};

ErrorCode emg_version_create(ModeBase** mode, SerialInterface* interface, ProcessManager* process_manager) {
    EMGVersionImpl* impl = (EMGVersionImpl*)malloc(sizeof(EMGVersionImpl));
    if (!impl) {
        log_error("Failed to allocate EMGVersionImpl");
        return ERROR_MEMORY_ALLOCATION;
    }
    
    memset(impl->version_string, 0, VERSION_STRING_MAX_LENGTH);
    
    ErrorCode error = mode_base_create(mode, interface, process_manager, &emg_version_vtable, impl);
    if (error != ERROR_NONE) {
        free(impl);
        return error;
    }
    
    return ERROR_NONE;
}

// Hardware Connection Check Implementation
static ErrorCode hardware_connection_execute_not_connected(ModeBase* mode, uint8_t* output, size_t* output_length) {
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

// VTable for Hardware Connection Check
static const ModeBaseVTable hardware_connection_vtable = {
    .get_mode_number = emg_version_get_number,
    .get_emg_config = emg_version_get_emg_config,
    .execute_mode = emg_version_execute,
    .execute_mode_not_connected = hardware_connection_execute_not_connected,
    .stop = emg_version_stop,
    .destroy = emg_version_destroy_impl
};

ErrorCode hardware_connection_create(ModeBase** mode, SerialInterface* interface, ProcessManager* process_manager) {
    EMGVersionImpl* impl = (EMGVersionImpl*)malloc(sizeof(EMGVersionImpl));
    if (!impl) {
        log_error("Failed to allocate EMGVersionImpl for hardware connection");
        return ERROR_MEMORY_ALLOCATION;
    }
    
    memset(impl->version_string, 0, VERSION_STRING_MAX_LENGTH);
    
    ErrorCode error = mode_base_create(mode, interface, process_manager, &hardware_connection_vtable, impl);
    if (error != ERROR_NONE) {
        free(impl);
        return error;
    }
    
    return ERROR_NONE;
}