#include "src/data/serialize_deserialize.h"
#include "src/core/logger.h"
#include "src/core/error_codes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// File format version for compatibility checking
static const uint32_t FILE_VERSION = 1;

// Magic numbers for validation
static const uint32_t SWEEP_MAGIC = 0x5357504D;  // "SWPM"
static const uint32_t STATE_MAGIC = 0x53544154;  // "STAT"
static const uint32_t PATIENT_MAGIC = 0x5054494E; // "PTIN"

// Helper functions for writing/reading basic types
static ErrorCode write_uint32(FILE* file, uint32_t value) {
    if (fwrite(&value, sizeof(uint32_t), 1, file) != 1) {
        return ERROR_FILE_WRITE;
    }
    return ERROR_NONE;
}

static ErrorCode read_uint32(FILE* file, uint32_t* value) {
    if (fread(value, sizeof(uint32_t), 1, file) != 1) {
        return ERROR_FILE_READ;
    }
    return ERROR_NONE;
}

static ErrorCode write_bool(FILE* file, bool value) {
    uint8_t byte = value ? 1 : 0;
    if (fwrite(&byte, sizeof(uint8_t), 1, file) != 1) {
        return ERROR_FILE_WRITE;
    }
    return ERROR_NONE;
}

static ErrorCode read_bool(FILE* file, bool* value) {
    uint8_t byte;
    if (fread(&byte, sizeof(uint8_t), 1, file) != 1) {
        return ERROR_FILE_READ;
    }
    *value = byte != 0;
    return ERROR_NONE;
}

static ErrorCode write_string(FILE* file, const char* str, size_t max_len) {
    size_t len = strlen(str);
    if (len >= max_len) len = max_len - 1;
    
    if (fwrite(&len, sizeof(size_t), 1, file) != 1) {
        return ERROR_FILE_WRITE;
    }
    if (fwrite(str, sizeof(char), len, file) != len) {
        return ERROR_FILE_WRITE;
    }
    return ERROR_NONE;
}

static ErrorCode read_string(FILE* file, char* str, size_t max_len) {
    size_t len;
    if (fread(&len, sizeof(size_t), 1, file) != 1) {
        return ERROR_FILE_READ;
    }
    
    if (len >= max_len) {
        return ERROR_BUFF_OVERFLOW;
    }
    
    if (fread(str, sizeof(char), len, file) != len) {
        return ERROR_FILE_READ;
    }
    str[len] = '\0';
    return ERROR_NONE;
}

// Sweep data serialization
ErrorCode sweep_data_serialize(const char* filepath, const SweepData* data) {
    if (!filepath || !data) {
        log_error("Invalid parameters in sweep_data_serialize");
        return ERROR_INVALID_PARAM;
    }

    FILE* file = fopen(filepath, "wb");
    if (!file) {
        log_error("Failed to open file for writing: %s", filepath);
        return ERROR_FILE_OPEN;
    }

    ErrorCode error = ERROR_NONE;

    // Write header
    error = write_uint32(file, SWEEP_MAGIC);
    if (error != ERROR_NONE) goto cleanup;
    
    error = write_uint32(file, FILE_VERSION);
    if (error != ERROR_NONE) goto cleanup;

    // Write sagittal data
    error = write_uint32(file, (uint32_t)data->sagittal.count);
    if (error != ERROR_NONE) goto cleanup;

    if (fwrite(data->sagittal.timestamps, sizeof(double), data->sagittal.count, file) != data->sagittal.count ||
        fwrite(data->sagittal.values, sizeof(double), data->sagittal.count, file) != data->sagittal.count) {
        error = ERROR_FILE_WRITE;
        goto cleanup;
    }

    // Write frontal data
    error = write_uint32(file, (uint32_t)data->frontal.count);
    if (error != ERROR_NONE) goto cleanup;

    if (fwrite(data->frontal.timestamps, sizeof(double), data->frontal.count, file) != data->frontal.count ||
        fwrite(data->frontal.values, sizeof(double), data->frontal.count, file) != data->frontal.count) {
        error = ERROR_FILE_WRITE;
        goto cleanup;
    }

    // Write metadata
    error = write_string(file, data->run_type, 32);
    if (error != ERROR_NONE) goto cleanup;

    error = write_string(file, data->timestamp, 64);
    if (error != ERROR_NONE) goto cleanup;

cleanup:
    fclose(file);
    if (error != ERROR_NONE) {
        log_error("Error in sweep_data_serialize: %d", error);
    }
    return error;
}

ErrorCode sweep_data_deserialize(const char* filepath, SweepData** data) {
    if (!filepath || !data) {
        log_error("Invalid parameters in sweep_data_deserialize");
        return ERROR_INVALID_PARAM;
    }

    FILE* file = fopen(filepath, "rb");
    if (!file) {
        log_error("Failed to open file for reading: %s", filepath);
        return ERROR_FILE_OPEN;
    }

    ErrorCode error = ERROR_NONE;
    SweepData* result = NULL;
    uint32_t magic, version;

    // Verify header
    error = read_uint32(file, &magic);
    if (error != ERROR_NONE) goto cleanup;
    if (magic != SWEEP_MAGIC) {
        error = ERROR_INVALID_FORMAT;
        goto cleanup;
    }

    error = read_uint32(file, &version);
    if (error != ERROR_NONE) goto cleanup;
    if (version != FILE_VERSION) {
        error = ERROR_VERSION_MISMATCH;
        goto cleanup;
    }

    // Allocate result
    result = (SweepData*)calloc(1, sizeof(SweepData));
    if (!result) {
        error = ERROR_MEMORY_ALLOCATION;
        goto cleanup;
    }

    // Read sagittal data
    uint32_t sagittal_count;
    error = read_uint32(file, &sagittal_count);
    if (error != ERROR_NONE) goto cleanup;

    result->sagittal.count = sagittal_count;
    result->sagittal.timestamps = (double*)malloc(sagittal_count * sizeof(double));
    result->sagittal.values = (double*)malloc(sagittal_count * sizeof(double));
    
    if (!result->sagittal.timestamps || !result->sagittal.values) {
        error = ERROR_MEMORY_ALLOCATION;
        goto cleanup;
    }

    if (fread(result->sagittal.timestamps, sizeof(double), sagittal_count, file) != sagittal_count ||
        fread(result->sagittal.values, sizeof(double), sagittal_count, file) != sagittal_count) {
        error = ERROR_FILE_READ;
        goto cleanup;
    }

    // Read frontal data
    uint32_t frontal_count;
    error = read_uint32(file, &frontal_count);
    if (error != ERROR_NONE) goto cleanup;

    result->frontal.count = frontal_count;
    result->frontal.timestamps = (double*)malloc(frontal_count * sizeof(double));
    result->frontal.values = (double*)malloc(frontal_count * sizeof(double));
    
    if (!result->frontal.timestamps || !result->frontal.values) {
        error = ERROR_MEMORY_ALLOCATION;
        goto cleanup;
    }

    if (fread(result->frontal.timestamps, sizeof(double), frontal_count, file) != frontal_count ||
        fread(result->frontal.values, sizeof(double), frontal_count, file) != frontal_count) {
        error = ERROR_FILE_READ;
        goto cleanup;
    }

    // Read metadata
    error = read_string(file, result->run_type, 32);
    if (error != ERROR_NONE) goto cleanup;

    error = read_string(file, result->timestamp, 64);
    if (error != ERROR_NONE) goto cleanup;

cleanup:
    fclose(file);
    if (error != ERROR_NONE) {
        sweep_data_free(result);
        result = NULL;
    }
    *data = result;
    return error;
}

void sweep_data_free(SweepData* data) {
    if (data) {
        free(data->sagittal.timestamps);
        free(data->sagittal.values);
        free(data->frontal.timestamps);
        free(data->frontal.values);
        free(data);
    }
}

// App state serialization
ErrorCode app_state_serialize(const char* filepath, const AppState* state) {
    if (!filepath || !state) {
        log_error("Invalid parameters in app_state_serialize");
        return ERROR_INVALID_PARAM;
    }

    FILE* file = fopen(filepath, "wb");
    if (!file) {
        log_error("Failed to open file for writing: %s", filepath);
        return ERROR_FILE_OPEN;
    }

    ErrorCode error = ERROR_NONE;

    error = write_uint32(file, STATE_MAGIC);
    if (error != ERROR_NONE) goto cleanup;
    
    error = write_uint32(file, FILE_VERSION);
    if (error != ERROR_NONE) goto cleanup;

    error = write_bool(file, state->exit_thread);
    if (error != ERROR_NONE) goto cleanup;

    error = write_string(file, state->event, 64);
    if (error != ERROR_NONE) goto cleanup;

    error = write_string(file, state->event_data, 256);
    if (error != ERROR_NONE) goto cleanup;

    error = write_bool(file, state->app_ready);
    if (error != ERROR_NONE) goto cleanup;

    error = write_string(file, state->requested_playback_file, 256);
    if (error != ERROR_NONE) goto cleanup;

cleanup:
    fclose(file);
    if (error != ERROR_NONE) {
        log_error("Error in app_state_serialize: %d", error);
    }
    return error;
}

ErrorCode app_state_deserialize(const char* filepath, AppState* state) {
    if (!filepath || !state) {
        log_error("Invalid parameters in app_state_deserialize");
        return ERROR_INVALID_PARAM;
    }

    FILE* file = fopen(filepath, "rb");
    if (!file) {
        log_error("Failed to open file for reading: %s", filepath);
        return ERROR_FILE_OPEN;
    }

    ErrorCode error = ERROR_NONE;
    uint32_t magic, version;

    error = read_uint32(file, &magic);
    if (error != ERROR_NONE) goto cleanup;
    if (magic != STATE_MAGIC) {
        error = ERROR_INVALID_FORMAT;
        goto cleanup;
    }

    error = read_uint32(file, &version);
    if (error != ERROR_NONE) goto cleanup;
    if (version != FILE_VERSION) {
        error = ERROR_VERSION_MISMATCH;
        goto cleanup;
    }

    error = read_bool(file, &state->exit_thread);
    if (error != ERROR_NONE) goto cleanup;

    error = read_string(file, state->event, 64);
    if (error != ERROR_NONE) goto cleanup;

    error = read_string(file, state->event_data, 256);
    if (error != ERROR_NONE) goto cleanup;

    error = read_bool(file, &state->app_ready);
    if (error != ERROR_NONE) goto cleanup;

    error = read_string(file, state->requested_playback_file, 256);
    if (error != ERROR_NONE) goto cleanup;

cleanup:
    fclose(file);
    if (error != ERROR_NONE) {
        log_error("Error in app_state_deserialize: %d", error);
    }
    return error;
}

// Patient info serialization
ErrorCode patient_info_serialize(const char* filepath, const PatientInfo* info) {
    if (!filepath || !info) {
        log_error("Invalid parameters in patient_info_serialize");
        return ERROR_INVALID_PARAM;
    }

    FILE* file = fopen(filepath, "wb");
    if (!file) {
        log_error("Failed to open file for writing: %s", filepath);
        return ERROR_FILE_OPEN;
    }

    ErrorCode error = ERROR_NONE;

    error = write_uint32(file, PATIENT_MAGIC);
    if (error != ERROR_NONE) goto cleanup;
    
    error = write_uint32(file, FILE_VERSION);
    if (error != ERROR_NONE) goto cleanup;

    error = write_string(file, info->name, 256);
    if (error != ERROR_NONE) goto cleanup;

    error = write_string(file, info->path, 1024);
    if (error != ERROR_NONE) goto cleanup;

cleanup:
    fclose(file);
    if (error != ERROR_NONE) {
        log_error("Error in patient_info_serialize: %d", error);
    }
    return error;
}

ErrorCode patient_info_deserialize(const char* filepath, PatientInfo* info) {
    if (!filepath || !info) {
        log_error("Invalid parameters in patient_info_deserialize");
        return ERROR_INVALID_PARAM;
    }

    FILE* file = fopen(filepath, "rb");
    if (!file) {
        log_error("Failed to open file for reading: %s", filepath);
        return ERROR_FILE_OPEN;
    }

    ErrorCode error = ERROR_NONE;
    uint32_t magic, version;

    error = read_uint32(file, &magic);
    if (error != ERROR_NONE) goto cleanup;
    if (magic != PATIENT_MAGIC) {
        error = ERROR_INVALID_FORMAT;
        goto cleanup;
    }

    error = read_uint32(file, &version);
    if (error != ERROR_NONE) goto cleanup;
    if (version != FILE_VERSION) {
        error = ERROR_VERSION_MISMATCH;
        goto cleanup;
    }

    error = read_string(file, info->name, 256);
    if (error != ERROR_NONE) goto cleanup;

    error = read_string(file, info->path, 1024);
    if (error != ERROR_NONE) goto cleanup;

cleanup:
    fclose(file);
    if (error != ERROR_NONE) {
        log_error("Error in patient_info_deserialize: %d", error);
    }
    return error;
}