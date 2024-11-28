#include <stdlib.h>
#include <string.h>
#include "byte_sync.h"
#include "logger.h"

// Block sizes for different channel types
#define CMS_BLOCK_SIZE 8
#define EMG_BLOCK_SIZE 16
#define EIGHT_CH_BLOCK_SIZE 16
#define TILT_BLOCK_SIZE 8
#define ESG_BLOCK_SIZE 4

void sync_result_init(SyncResult* result) {
    if (!result) {
        log_warning("Attempted to initialize NULL sync result");
        return;
    }
    result->synced_data = NULL;
    result->synced_length = 0;
    result->found_sync = false;
    log_debug("Sync result initialized");
}

void sync_result_free(SyncResult* result) {
    if (!result) {
        log_warning("Attempted to free NULL sync result");
        return;
    }
    free(result->synced_data);
    result->synced_data = NULL;
    result->synced_length = 0;
    result->found_sync = false;
    log_debug("Sync result freed");
}

bool sync_emg_channels(const uint8_t* data, size_t length) {
    if (!data || length < EMG_BLOCK_SIZE) {
        log_error("Invalid parameters in sync_emg_channels");
        return false;
    }

    // Check EMG channel sequence (iteration + 8 in high nibble)
    for (size_t i = 0; i < EMG_BLOCK_SIZE; i += 2) {
        uint8_t high_nibble = (data[i] >> 4) & 0x0F;
        uint8_t expected = (i/2) + 8;
        if (high_nibble != expected) {
            log_debug("EMG channel sync failed at position %zu: expected %u, got %u", 
                     i, expected, high_nibble);
            return false;
        }
    }

    log_debug("EMG channels synced successfully");
    return true;
}

bool sync_cms_channels(const uint8_t* data, size_t length) {
    if (!data || length < CMS_BLOCK_SIZE) {
        log_error("Invalid parameters in sync_cms_channels");
        return false;
    }

    // Check CMS channel sequence (iteration in high nibble)
    for (size_t i = 0; i < CMS_BLOCK_SIZE; i += 2) {
        uint8_t high_nibble = (data[i] >> 4) & 0x0F;
        uint8_t expected = i/2;
        if (high_nibble != expected) {
            log_debug("CMS channel sync failed at position %zu: expected %u, got %u", 
                     i, expected, high_nibble);
            return false;
        }
    }

    log_debug("CMS channels synced successfully");
    return true;
}

bool sync_8_channels(const uint8_t* data, size_t length) {
    if (!data || length < EIGHT_CH_BLOCK_SIZE) {
        log_error("Invalid parameters in sync_8_channels");
        return false;
    }

    // Check 8-channel sequence (iteration in high nibble)
    for (size_t i = 0; i < EIGHT_CH_BLOCK_SIZE; i += 2) {
        uint8_t high_nibble = (data[i] >> 4) & 0x0F;
        uint8_t expected = i/2;
        if (high_nibble != expected) {
            log_debug("8-channel sync failed at position %zu: expected %u, got %u", 
                     i, expected, high_nibble);
            return false;
        }
    }

    log_debug("8 channels synced successfully");
    return true;
}

bool sync_tilt_channels(const uint8_t* data, size_t length) {
    if (!data || length < TILT_BLOCK_SIZE) {
        log_error("Invalid parameters in sync_tilt_channels");
        return false;
    }

    // Check tilt channel pattern [0,2,0,7]
    const uint8_t expected_pattern[] = {0, 2, 0, 7};
    for (size_t i = 0; i < TILT_BLOCK_SIZE; i += 2) {
        uint8_t high_nibble = (data[i] >> 4) & 0x0F;
        uint8_t expected = expected_pattern[i/2];
        if (high_nibble != expected) {
            log_debug("Tilt channel sync failed at position %zu: expected %u, got %u", 
                     i, expected, high_nibble);
            return false;
        }
    }

    log_debug("Tilt channels synced successfully");
    return true;
}

bool sync_esg_channels(const uint8_t* data, size_t length) {
    if (!data || length < ESG_BLOCK_SIZE) {
        log_error("Invalid parameters in sync_esg_channels");
        return false;
    }

    // Check ESG channel sequence (iteration + 4 in high nibble)
    for (size_t i = 0; i < ESG_BLOCK_SIZE; i += 2) {
        uint8_t high_nibble = (data[i] >> 4) & 0x0F;
        uint8_t expected = (i/2) + 4;
        if (high_nibble != expected) {
            log_debug("ESG channel sync failed at position %zu: expected %u, got %u", 
                     i, expected, high_nibble);
            return false;
        }
    }

    log_debug("ESG channels synced successfully");
    return true;
}

static ErrorCode allocate_sync_buffer(SyncResult* result, size_t max_length) {
    result->synced_data = (uint8_t*)malloc(max_length);
    if (!result->synced_data) {
        log_error("Failed to allocate sync buffer of size %zu", max_length);
        set_last_error(ERROR_MEMORY_ALLOCATION);
        return ERROR_MEMORY_ALLOCATION;
    }
    log_debug("Sync buffer allocated successfully, size: %zu", max_length);
    return ERROR_NONE;
}

ErrorCode resync_bytes(const uint8_t* data, 
                      size_t length,
                      size_t block_size,
                      bool (*sync_func1)(const uint8_t*, size_t),
                      bool (*sync_func2)(const uint8_t*, size_t),
                      size_t sync1_offset,
                      size_t sync2_offset,
                      SyncResult* result) {
    if (!data || !result || !sync_func1 || length < block_size) {
        set_last_error(ERROR_INVALID_PARAMETER);
        log_error("Invalid parameters in resync_bytes");
        return ERROR_INVALID_PARAMETER;
    }

    log_debug("Starting byte resync, data length: %zu, block size: %zu", length, block_size);
    sync_result_init(result);
    
    ErrorCode err = allocate_sync_buffer(result, length);
    if (err != ERROR_NONE) {
        return err;
    }

    size_t i = 0;
    size_t output_index = 0;
    bool found_first_set = false;

    while (i <= length - block_size) {
        bool sync1_valid = sync_func1(data + i + sync1_offset, length - i - sync1_offset);
        bool sync2_valid = true;  // Default to true if no second sync function

        if (sync_func2) {
            sync2_valid = sync_func2(data + i + sync2_offset, length - i - sync2_offset);
        }

        if (sync1_valid && sync2_valid) {
            memcpy(result->synced_data + output_index, data + i, block_size);
            output_index += block_size;
            found_first_set = true;
            i += block_size;
            log_debug("Found sync at position %zu", i - block_size);
        } else if (found_first_set) {
            log_debug("Sync lost after %zu bytes", output_index);
            break;
        } else {
            i++;
        }
    }

    result->synced_length = output_index;
    result->found_sync = found_first_set;

    if (found_first_set) {
        log_debug("Resync successful, synced %zu bytes", output_index);
    } else {
        log_warning("Resync failed to find sync pattern");
    }

    set_last_error(ERROR_NONE);
    return ERROR_NONE;
}