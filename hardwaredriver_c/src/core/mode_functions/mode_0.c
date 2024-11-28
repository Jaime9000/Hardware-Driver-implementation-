#include <stdlib.h>
#include <string.h>
#include "mode_0.h"
#include "logger.h"
#include <math.h>
#include <time.h>

// VTable implementations
static int get_mode_number(const Mode* mode) {
    (void)mode;  // Unused parameter
    return 44;
}

static const uint8_t* get_emg_config(const Mode* mode, size_t* length) {
    static const uint8_t config[] = {'r'};
    *length = sizeof(config);
    return config;
}

static const ModeVTable mode_0_vtable = {
    .get_mode_number = get_mode_number,
    .get_emg_config = get_emg_config
};

// Helper functions
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
    
    // Calculate mean
    for (size_t i = 0; i < length; i++) {
        sum += values[i];
    }
    mean = sum / length;
    
    // Calculate variance
    for (size_t i = 0; i < length; i++) {
        variance += pow(values[i] - mean, 2);
    }
    
    return variance / length;
}

ErrorCode mode_0_create(Mode0** mode, SerialInterface* interface) {
    if (!mode || !interface) {
        log_error("Invalid parameters in mode_0_create");
        return ERROR_INVALID_PARAMETER;
    }

    Mode0* new_mode = (Mode0*)malloc(sizeof(Mode0));
    if (!new_mode) {
        log_error("Failed to allocate Mode0");
        return ERROR_MEMORY_ALLOCATION;
    }

    ErrorCode error = mode_init(&new_mode->base, interface, &mode_0_vtable, new_mode);
    if (error != ERROR_NONE) {
        free(new_mode);
        return error;
    }

    // Initialize Mode0 specific members
    for (int i = 0; i < MODE_0_CHANNEL_COUNT; i++) {
        new_mode->align_values[i] = MODE_0_DEFAULT_ALIGN_VALUE;
        new_mode->offset_values[i] = 0;
        new_mode->prev_data_array[i] = 0;
    }
    new_mode->has_prev_data = false;
    new_mode->is_first_run = true;

    *mode = new_mode;
    log_debug("Mode 0 created successfully");
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
        return ERROR_INVALID_PARAMETER;
    }

    for (int i = 0; i < MODE_0_CHANNEL_COUNT; i++) {
        uint8_t n_low = values[i * 2];
        uint8_t n_high = values[i * 2 + 1];
        uint16_t scaled_value = scale_low_value(n_low, n_high);
        
        int16_t computed_value = scaled_value - mode->align_values[i] - mode->offset_values[i];

        // Special handling for lateral channel
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

ErrorCode mode_0_execute(Mode0* mode, uint8_t* output, size_t* output_length) {
    if (!mode || !output || !output_length) {
        log_error("Invalid parameters in mode_0_execute");
        return ERROR_INVALID_PARAMETER;
    }

    uint8_t read_buffer[MODE_0_READ_SIZE];
    size_t bytes_read;
    
    ErrorCode error = serial_interface_read(mode->base.interface, read_buffer, MODE_0_READ_SIZE, &bytes_read);
    if (error != ERROR_NONE) {
        return error;
    }

    SyncResult sync_result;
    error = resync_bytes(read_buffer, bytes_read, MODE_0_BLOCK_SIZE, 
                        sync_cms_channels, NULL, 0, 0, &sync_result);
    
    if (!sync_result.found_sync) {
        log_error("Failed to sync bytes in Mode 0");
        sync_result_free(&sync_result);
        return ERROR_SYNC_FAILED;
    }

    size_t output_index = 0;
    const uint8_t* values = sync_result.synced_data;
    size_t remaining = sync_result.synced_length;

    while (remaining >= MODE_0_BLOCK_SIZE) {
        int16_t data_array[MODE_0_CHANNEL_COUNT];
        error = mode_0_process_values(mode, values, data_array);
        
        if (error != ERROR_NONE) {
            sync_result_free(&sync_result);
            return error;
        }

        // Check for significant changes if we have previous data
        bool should_append = true;
        if (mode->has_prev_data) {
            should_append = false;
            for (int i = 0; i < MODE_0_CHANNEL_COUNT; i++) {
                if (abs(data_array[i] - mode->prev_data_array[i]) > 2) {
                    should_append = true;
                    break;
                }
            }
        }

        if (should_append) {
            memcpy(mode->prev_data_array, data_array, sizeof(data_array));
            mode->has_prev_data = true;
            
            // Pack data into output buffer
            memcpy(output + output_index, data_array, sizeof(data_array));
            output_index += sizeof(data_array);
        }

        values += MODE_0_BLOCK_SIZE;
        remaining -= MODE_0_BLOCK_SIZE;
    }

    sync_result_free(&sync_result);
    *output_length = output_index;

    // Flush any remaining data
    serial_interface_flush(mode->base.interface);
    
    return ERROR_NONE;
}

ErrorCode mode_0_execute_not_connected(Mode0* mode, uint8_t* output, size_t* output_length) {
    if (!mode || !output || !output_length) {
        log_error("Invalid parameters in mode_0_execute_not_connected");
        return ERROR_INVALID_PARAMETER;
    }

    // Generate dummy data pattern
    uint8_t pattern[8] = {
        0x00, 0x00,             // Channel 0
        0x10, 0x00,             // Channel 1
        0x20, 0x00,             // Channel 2
        0x30, 0x00              // Channel 3
    };

    size_t output_index = 0;
    for (int i = 0; i < 400 && output_index < *output_length; i++) {
        memcpy(output + output_index, pattern, sizeof(pattern));
        output_index += sizeof(pattern);
    }

    *output_length = output_index;
    return ERROR_NONE;
}

// Raw Mode Implementation
ErrorCode mode_0_raw_create(Mode0Raw** mode, SerialInterface* interface) {
    if (!mode || !interface) {
        log_error("Invalid parameters in mode_0_raw_create");
        return ERROR_INVALID_PARAMETER;
    }

    Mode0Raw* new_mode = (Mode0Raw*)malloc(sizeof(Mode0Raw));
    if (!new_mode) {
        log_error("Failed to allocate Mode0Raw");
        return ERROR_MEMORY_ALLOCATION;
    }

    ErrorCode error = mode_0_create(&new_mode->base, interface);
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
    ErrorCode error = resync_bytes(data, length, MODE_0_BLOCK_SIZE,
                                 sync_cms_channels, NULL, 0, 0, &sync_result);
    
    if (!sync_result.found_sync) {
        sync_result_free(&sync_result);
        return false;
    }

    // Calculate variance for each position in the block
    double max_variance = 0.0;
    for (size_t pos = 0; pos < MODE_0_BLOCK_SIZE; pos++) {
        uint8_t values[20];  // Store values for variance calculation
        size_t value_count = 0;
        
        // Collect values at the same position from different blocks
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
        return ERROR_INVALID_PARAMETER;
    }

    uint8_t read_buffer[1600];
    size_t bytes_read;

    // Handle first run initialization
    if (mode->is_first_run) {
        int ignore_count = 0;
        size_t bytes_thrown = 0;
        
        while (bytes_thrown < 32000) {
            sleep_ms(10);
            
            ErrorCode error = serial_interface_read(mode->base.base.interface, 
                                                  read_buffer, 1600, &bytes_read);
            if (error != ERROR_NONE) continue;

            bytes_thrown += bytes_read;
            
            if (wait_for_init(read_buffer, bytes_read)) {
                ignore_count++;
            }
            
            if (ignore_count > 25) break;
        }
        
        mode->is_first_run = false;
    }

    // Read and process data
    ErrorCode error = serial_interface_read(mode->base.base.interface, 
                                          read_buffer, 1600, &bytes_read);
    if (error != ERROR_NONE) {
        return error;
    }

    SyncResult sync_result;
    error = resync_bytes(read_buffer, bytes_read, MODE_0_BLOCK_SIZE,
                        sync_cms_channels, NULL, 0, 0, &sync_result);

    if (!sync_result.found_sync || sync_result.synced_length < 160) {
        // Try reading more data if we didn't get enough
        error = serial_interface_read(mode->base.base.interface, 
                                    read_buffer, 320, &bytes_read);
        if (error != ERROR_NONE) {
            sync_result_free(&sync_result);
            return error;
        }

        sync_result_free(&sync_result);
        error = resync_bytes(read_buffer, bytes_read, MODE_0_BLOCK_SIZE,
                           sync_cms_channels, NULL, 0, 0, &sync_result);
    }

    if (!sync_result.found_sync) {
        log_error("Cannot verify byte order in raw mode");
        sync_result_free(&sync_result);
        return ERROR_SYNC_FAILED;
    }

    // Copy synced data to output
    size_t copy_size = sync_result.synced_length;
    if (copy_size > *output_length) {
        copy_size = *output_length;
    }
    
    memcpy(output, sync_result.synced_data, copy_size);
    *output_length = copy_size;

    sync_result_free(&sync_result);
    return ERROR_NONE;
}