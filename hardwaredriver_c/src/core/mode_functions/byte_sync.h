#ifndef BYTE_SYNC_H
#define BYTE_SYNC_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "error_codes.h"
#include "logger.h"

// Structure to hold resync results
typedef struct {
    uint8_t* synced_data;
    size_t synced_length;
    bool found_sync;
} SyncResult;

// Channel sync functions (matching Python implementation)
bool sync_emg_channels(const uint8_t* data, size_t length);    // iteration + 8 in high nibble
bool sync_cms_channels(const uint8_t* data, size_t length);    // iteration in high nibble
bool sync_8_channels(const uint8_t* data, size_t length);      // iteration in high nibble
bool sync_tilt_channels(const uint8_t* data, size_t length);   // specific pattern [0,2,0,7]
bool sync_esg_channels(const uint8_t* data, size_t length);    // iteration + 4 in high nibble

// Generic resync function that can be used by any mode
ErrorCode resync_bytes(const uint8_t* data, 
                      size_t length,
                      size_t block_size,
                      bool (*sync_func1)(const uint8_t*, size_t),
                      bool (*sync_func2)(const uint8_t*, size_t),
                      size_t sync1_offset,
                      size_t sync2_offset,
                      SyncResult* result);

// Utility functions
void sync_result_init(SyncResult* result);
void sync_result_free(SyncResult* result);

#endif // BYTE_SYNC_H