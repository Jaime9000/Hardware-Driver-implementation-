#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include "mode_0.h"
#include "logger.h"

static int get_mode_number(const ModeBase* mode) {
    (void)mode;
    return 44;
}

static const uint8_t* get_emg_config(const ModeBase* mode, size_t* length) {
    static const uint8_t config[] = {'r'};
    *length = sizeof(config);
    return config;
}

static uint16_t scale_low_value(uint8_t n_low, uint8_t n_high) {
    return ((n_low & 15) << 8) | n_high;
}

static void sleep_ms(int milliseconds) {
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

static double calculate_variance(const uint8_t* values, size_t length) {
    if (length == 0) return 0.0;
    
    double sum = 0.0, mean, variance = 0.0;
    
    for (size_t i = 0; i < length; i++) {
        sum += values[i];
    }
    mean = sum / length;
    
    for (size_t i = 0; i < length; i++) {
        variance += pow(values[i] - mean, 2);
    }
    
    return variance / length;
}

static ErrorCode mode_0_execute(ModeBase* mode, uint8_t* output, size_t* output_length) {
    Mode0* impl = (Mode0*)mode->impl;
    uint8_t read_buffer[160];
    size_t bytes_read;
    
    ErrorCode error = serial_interface_read_data(mode->interface, read_buffer, &bytes_read, MODE_0_READ_SIZE);
    if (error != ERROR_NONE) {
        return error;
    }

    SyncResult sync_result;
    error = resync_bytes(read_buffer, bytes_read, MODE_0_BLOCK_SIZE, sync_cms_channels, NULL, 0, 0, &sync_result);
    if (!sync_result.found_sync) {
        sync_result_free(&sync_result);
        return ERROR_SYNC_FAILED;
    }

    int16_t data_array[MODE_0_CHANNEL_COUNT];
    error = mode_0_process_values(impl, sync_result.synced_data, data_array);
    if (error != ERROR_NONE) {
        sync_result_free(&sync_result);
        return error;
    }

    memcpy(output, data_array, sizeof(data_array));
    *output_length = sizeof(data_array);

    sync_result_free(&sync_result);
    return ERROR_NONE;
}

static ErrorCode mode_0_execute_not_connected(ModeBase* mode, uint8_t* output, size_t* output_length) {
    uint8_t in_values[] = {0, 0, 1 << 4, 0, 2 << 4, 0, 3 << 4, 0};
    for (int i = 0; i < 400; i++) {
        memcpy(output + i * sizeof(in_values), in_values, sizeof(in_values));
    }
    *output_length = 400 * sizeof(in_values);
    return ERROR_NONE;
}

static void mode_0_stop(ModeBase* mode) {
    (void)mode;
}

static void mode_0_destroy_impl(ModeBase* mode) {
    if (mode && mode->impl) {
        free(mode->impl);
        mode->impl = NULL;
    }
}

static const ModeBaseVTable mode_0_vtable = {
    .get_mode_number = get_mode_number,
    .get_emg_config = get_emg_config,
    .execute_mode = mode_0_execute,
    .execute_mode_not_connected = mode_0_execute_not_connected,
    .stop = mode_0_stop,
    .destroy = mode_0_destroy_impl
};

ErrorCode mode_0_create(ModeBase** mode, SerialInterface*serial_interface, ProcessManager* process_manager) {
    Mode0* impl = (Mode0*)malloc(sizeof(Mode0));
    if (!impl) {
        log_error("Failed to allocate Mode0");
        return ERROR_MEMORY_ALLOCATION;
    }
    
    memset(impl->align_values, MODE_0_DEFAULT_ALIGN_VALUE, sizeof(impl->align_values));
    memset(impl->offset_values, 0, sizeof(impl->offset_values));
    memset(impl->prev_data_array, 0, sizeof(impl->prev_data_array));
    impl->has_prev_data = false;
    impl->is_first_run = true;
    
    ErrorCode error = mode_base_create(mode,serial_interface, process_manager, &mode_0_vtable, impl);
    if (error != ERROR_NONE) {
        free(impl);
        return error;
    }
    
    return ERROR_NONE;
}

void mode_0_destroy(Mode0* mode) {
    if (mode) {
        log_debug("Destroying Mode 0");
        free(mode);
    }
}

ErrorCode mode_0_process_values(Mode0* mode, const uint8_t* values, int16_t* data_array) {
    if (!mode || !values || !data_array) {
        log_error("Invalid parameters in mode_0_process_values");
        return ERROR_INVALID_PARAM;
    }

    for (int i = 0; i < MODE_0_CHANNEL_COUNT; i++) {
        uint8_t n_low = values[i * 2];
        uint8_t n_high = values[i * 2 + 1];
        uint16_t scaled_value = scale_low_value(n_low, n_high);
        
        int16_t computed_value = scaled_value - mode->align_values[i] - mode->offset_values[i];

        if (i == MODE_0_LATERAL_CHANNEL_INDEX) {
            int32_t reduction_factor = 
                abs(MODE_0_DEFAULT_ALIGN_VALUE - mode->align_values[0] - data_array[0] - mode->offset_values[0]) +
                abs(MODE_0_DEFAULT_ALIGN_VALUE - mode->align_values[1] - data_array[1] - mode->offset_values[1]);
            computed_value -= (reduction_factor >> 13);
        }

        data_array[i] = computed_value;
    }

    return ERROR_NONE;
}

ErrorCode mode_0_raw_create(Mode0Raw** mode, SerialInterface*serial_interface, ProcessManager* process_manager) {
    if (!mode || !interface) {
        log_error("Invalid parameters in mode_0_raw_create");
        return ERROR_INVALID_PARAM;
    }

    Mode0Raw* new_mode = (Mode0Raw*)malloc(sizeof(Mode0Raw));
    if (!new_mode) {
        log_error("Failed to allocate Mode0Raw");
        return ERROR_MEMORY_ALLOCATION;
    }

    ErrorCode error = mode_0_create(&new_mode->base,serial_interface, process_manager);
    if (error != ERROR_NONE) {
        free(new_mode);
        return error;
    }

    new_mode->is_first_run = true;
    *mode = new_mode;
    log_debug("Mode 0 Raw created successfully");
    return ERROR_NONE;
}

void mode_0_raw_destroy(Mode0Raw* mode) {
    if (mode) {
        log_debug("Destroying Mode 0 Raw");
        mode_0_destroy(&mode->base);
        free(mode);
    }
}

static bool wait_for_init(const uint8_t* data, size_t length) {
    if (length < MODE_0_BLOCK_SIZE) return false;

    SyncResult sync_result;
    ErrorCode error = resync_bytes(data, length, MODE_0_BLOCK_SIZE, sync_cms_channels, NULL, 0, 0, &sync_result);
    
    if (!sync_result.found_sync) {
        sync_result_free(&sync_result);
        return false;
    }

    double max_variance = 0.0;
    for (size_t pos = 0; pos < MODE_0_BLOCK_SIZE; pos++) {
        uint8_t values[20];
        size_t value_count = 0;
        
        for (size_t i = pos; i < sync_result.synced_length; i += MODE_0_BLOCK_SIZE) {
            if (value_count < 20) {
                values[value_count++] = sync_result.synced_data[i];
            }
        }
        
        double var = calculate_variance(values, value_count);
        if (var > max_variance) {
            max_variance = var;
        }
    }

    sync_result_free(&sync_result);
    return max_variance < 3.0;
}

ErrorCode mode_0_raw_execute(Mode0Raw* mode, uint8_t* output, size_t* output_length) {
    if (!mode || !output || !output_length) {
        log_error("Invalid parameters in mode_0_raw_execute");
        return ERROR_INVALID_PARAM;
    }

    uint8_t read_buffer[1600];
    size_t bytes_read;

    if (mode->is_first_run) {
        int ignore_count = 0;
        size_t bytes_thrown = 0;
        
        while (bytes_thrown < 32000) {
            sleep_ms(10);
            
            ErrorCode error = serial_interface_read(mode->base.base.interface, read_buffer, 1600, &bytes_read);
            if (error != ERROR_NONE) continue;

            bytes_thrown += bytes_read;
            
            if (wait_for_init(read_buffer, bytes_read)) {
                ignore_count++;
            }
            
            if (ignore_count > 25) break;
        }
        
        mode->is_first_run = false;
    }

    ErrorCode error = serial_interface_read(mode->base.base.interface, read_buffer, 1600, &bytes_read);
    if (error != ERROR_NONE) {
        return error;
    }

    SyncResult sync_result;
    error = resync_bytes(read_buffer, bytes_read, MODE_0_BLOCK_SIZE, sync_cms_channels, NULL, 0, 0, &sync_result);

    if (!sync_result.found_sync || sync_result.synced_length < 160) {
        error = serial_interface_read(mode->base.base.interface, read_buffer, 320, &bytes_read);
        if (error != ERROR_NONE) {
            sync_result_free(&sync_result);
            return error;
        }

        sync_result_free(&sync_result);
        error = resync_bytes(read_buffer, bytes_read, MODE_0_BLOCK_SIZE, sync_cms_channels, NULL, 0, 0, &sync_result);
    }

    if (!sync_result.found_sync) {
        log_error("Cannot verify byte order in raw mode");
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

// Add to existing mode_0.c mode_0_align functions
static ErrorCode mode_0_align_process_values(Mode0Align* mode, const uint8_t* values, int16_t* data_array) {
    // First get the normal processed values
    ErrorCode error = mode_0_process_values(&mode->base, values, data_array);
    if (error != ERROR_NONE) {
        return error;
    }

    // Calculate and store new alignment values
    for (int i = 0; i < MODE_0_CHANNEL_COUNT; i++) {
        mode->current_aligned_values[i] = data_array[i] + MODE_0_DEFAULT_ALIGN_VALUE;
    }
    mode->has_aligned_values = true;

    return ERROR_NONE;
}

static ErrorCode mode_0_align_execute(ModeBase* mode, uint8_t* output, size_t* output_length) {
    Mode0Align* impl = (Mode0Align*)mode->impl;
    ErrorCode error = mode_0_execute(mode, output, output_length);
    
    // The base execute function has already processed and stored the output
    // We just need to ensure our alignment values are calculated
    if (error == ERROR_NONE && impl->has_aligned_values) {
        // Update the global alignment values
        memcpy(START_ALIGN_VALUES, impl->current_aligned_values, 
               sizeof(int16_t) * MODE_0_CHANNEL_COUNT);
    }
    
    return error;
}

static const ModeBaseVTable mode_0_align_vtable = {
    .get_mode_number = get_mode_number,
    .get_emg_config = get_emg_config,
    .execute_mode = mode_0_align_execute,
    .execute_mode_not_connected = mode_0_execute_not_connected,
    .stop = mode_0_stop,
    .destroy = mode_0_align_destroy_impl
};

ErrorCode mode_0_align_create(Mode0Align** mode, SerialInterface*serial_interface, ProcessManager* process_manager) {
    Mode0Align* new_mode = (Mode0Align*)malloc(sizeof(Mode0Align));
    if (!new_mode) {
        log_error("Failed to allocate Mode0Align");
        return ERROR_MEMORY_ALLOCATION;
    }

    ErrorCode error = mode_0_create(&new_mode->base,serial_interface, process_manager);
    if (error != ERROR_NONE) {
        free(new_mode);
        return error;
    }

    new_mode->has_aligned_values = false;
    memset(new_mode->current_aligned_values, 0, sizeof(new_mode->current_aligned_values));

    *mode = new_mode;
    return ERROR_NONE;
}

void mode_0_align_destroy(Mode0Align* mode) {
    if (mode) {
        // If we have new alignment values, apply them before destroying
        if (mode->has_aligned_values) {
            memcpy(START_ALIGN_VALUES, mode->current_aligned_values, 
                   sizeof(int16_t) * MODE_0_CHANNEL_COUNT);
        }
        mode_0_destroy(&mode->base);
        free(mode);
    }
}
