#include "mode_sweep.h"
#include "serial_interface.h"
#include "byte_sync.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

// Private function declarations
static ErrorCode execute(Mode* base, uint8_t* output, size_t* output_length);
static void destroy(Mode* base);
static ErrorCode re_sync_bytes(const uint8_t* raw_data, size_t raw_length, 
                             uint8_t* synced_data, size_t* synced_length);

// Implementation
ErrorCode mode_sweep_create(Mode** mode) {
    ModeSweep* sweep = (ModeSweep*)calloc(1, sizeof(ModeSweep));
    if (!sweep) return ERROR_MEMORY_ALLOCATION;

    sweep->base.impl = sweep;
    sweep->base.execute = execute;
    sweep->base.destroy = destroy;
    sweep->is_first_run = true;
    sweep->front_angle = 0.0;
    sweep->side_angle = 0.0;

    *mode = (Mode*)sweep;
    return ERROR_NONE;
}

ErrorCode mode_sweep_compute_angle(double ref, double axis1, double axis2, double* result) {
    if (!result) return ERROR_INVALID_PARAMETER;
    
    double sum = sqrt((axis1 * axis1) + (axis2 * axis2));
    if (sum == 0) {
        *result = 0;
        return ERROR_NONE;
    }

    *result = atan(ref / sum) * SCALING_FACTOR;
    return ERROR_NONE;
}

ErrorCode mode_sweep_compute_tilt_data(const uint8_t* tilt_values, double* front_angle, double* side_angle) {
    if (!tilt_values || !front_angle || !side_angle) return ERROR_INVALID_PARAMETER;

    // Process 8 bytes of tilt data into 4 values
    int16_t computed[4] = {0};
    
    for (int i = 0; i < 8; i += 2) {
        uint8_t n_low = tilt_values[i];
        uint8_t n_high = tilt_values[i + 1];
        
        int16_t scaled_value = ((n_low & 255) << 8) | n_high;
        
        // Check negative number for first 3 values
        if (i < 6 && (scaled_value & 0x8000)) {
            scaled_value = ((scaled_value ^ 0xFFFF) + 1) * -1;
        }
        computed[i/2] = scaled_value;
    }

    // Calculate front angle
    ErrorCode error = mode_sweep_compute_angle(computed[1], computed[0], computed[2], front_angle);
    if (error != ERROR_NONE) return error;

    if (computed[1] < 0) {
        *front_angle *= -1;
    }

    // Calculate side angle
    error = mode_sweep_compute_angle(computed[2], computed[0], computed[1], side_angle);
    if (error != ERROR_NONE) return error;

    if (computed[2] >= 0) {
        *side_angle *= -1;
    }

    return ERROR_NONE;
}

static ErrorCode re_sync_bytes(const uint8_t* raw_data, size_t raw_length, 
                             uint8_t* synced_data, size_t* synced_length) {
    if (!raw_data || !synced_data || !synced_length) return ERROR_INVALID_PARAMETER;
    
    size_t i = 0;
    size_t final_size = 0;
    bool found_first_set = false;

    while (i < raw_length && (raw_length - i) >= 16) {
        SyncResult sync_result;
        ErrorCode error = byte_sync_check(raw_data + i, 8, NULL, 0, 0, &sync_result);
        if (error != ERROR_NONE) return error;

        if (sync_result.found_sync) {
            memcpy(synced_data + final_size, raw_data + i, 16);
            final_size += 16;
            found_first_set = true;
            i += 16;
        } else if (found_first_set) {
            break;
        } else {
            i++;
        }
        
        sync_result_free(&sync_result);
    }

    *synced_length = final_size;
    return ERROR_NONE;
}

static ErrorCode execute(Mode* base, uint8_t* output, size_t* output_length) {
    ModeSweep* mode = (ModeSweep*)base->impl;
    
    // Handle first run initialization
    if (mode->is_first_run) {
        uint8_t read_buffer[MODE_SWEEP_READ_SIZE];
        
        for (int i = 0; i < 5; i++) {
            size_t bytes_read;
            ErrorCode error = serial_interface_read(mode->base.interface, 
                                                  read_buffer,
                                                  MODE_SWEEP_READ_SIZE, 
                                                  &bytes_read);
            if (error != ERROR_NONE) return error;
        }
        mode->is_first_run = false;
    }

    // Read raw data
    uint8_t raw_data[MODE_SWEEP_READ_SIZE];
    size_t bytes_read;
    ErrorCode error = serial_interface_read(mode->base.interface,
                                          raw_data,
                                          MODE_SWEEP_READ_SIZE,
                                          &bytes_read);
    if (error != ERROR_NONE) return error;

    // Re-sync bytes
    uint8_t synced_data[MODE_SWEEP_READ_SIZE];
    size_t synced_length;
    error = re_sync_bytes(raw_data, bytes_read, synced_data, &synced_length);
    if (error != ERROR_NONE) return error;

    if (synced_length == 0) {
        return ERROR_INVALID_DATA;
    }

    // Process tilt data
    double front_angles[MODE_SWEEP_READ_SIZE/16];
    double side_angles[MODE_SWEEP_READ_SIZE/16];
    size_t angle_count = 0;

    for (size_t i = 0; i < synced_length; i += 16) {
        error = mode_sweep_compute_tilt_data(synced_data + i + 8,
                                           &front_angles[angle_count],
                                           &side_angles[angle_count]);
        if (error != ERROR_NONE) return error;
        angle_count++;
    }

    // Calculate means
    if (angle_count > 0) {
        double front_sum = 0, side_sum = 0;
        for (size_t i = 0; i < angle_count; i++) {
            front_sum += front_angles[i];
            side_sum += side_angles[i];
        }
        mode->front_angle = front_sum / angle_count;
        mode->side_angle = side_sum / angle_count;
    }

    // Copy synced data to output
    memcpy(output, synced_data, synced_length);
    *output_length = synced_length;

    return ERROR_NONE;
}

static void destroy(Mode* base) {
    if (base) {
        ModeSweep* mode = (ModeSweep*)base->impl;
        free(mode);
    }
}