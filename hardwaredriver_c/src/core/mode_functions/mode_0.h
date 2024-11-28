#ifndef MODE_0_H
#define MODE_0_H

#include <stdbool.h>
#include <stdint.h>
#include "mode.h"
#include "serial_interface.h"
#include "byte_sync.h"
#include "error_codes.h"

// Constants
#define MODE_0_CHANNEL_COUNT 4
#define MODE_0_BLOCK_SIZE 8
#define MODE_0_READ_SIZE 160
#define MODE_0_DEFAULT_ALIGN_VALUE 2048
#define MODE_0_LATERAL_CHANNEL_INDEX 2

// Mode 0 implementation structure
typedef struct {
    Mode base;
    int16_t align_values[MODE_0_CHANNEL_COUNT];
    int16_t offset_values[MODE_0_CHANNEL_COUNT];
    int16_t prev_data_array[MODE_0_CHANNEL_COUNT];
    bool has_prev_data;
    bool is_first_run;
} Mode0;

// Constructor/Destructor
ErrorCode mode_0_create(Mode0** mode, SerialInterface* interface);
void mode_0_destroy(Mode0* mode);

// Mode 0 specific functions
ErrorCode mode_0_init_align_values(Mode0* mode);
ErrorCode mode_0_process_values(Mode0* mode, const uint8_t* values, int16_t* data_array);
ErrorCode mode_0_execute(Mode0* mode, uint8_t* output, size_t* output_length);
ErrorCode mode_0_execute_not_connected(Mode0* mode, uint8_t* output, size_t* output_length);

// Raw mode variant
typedef struct {
    Mode0 base;
    bool is_first_run;
} Mode0Raw;

ErrorCode mode_0_raw_create(Mode0Raw** mode, SerialInterface* interface);
void mode_0_raw_destroy(Mode0Raw* mode);
ErrorCode mode_0_raw_execute(Mode0Raw* mode, uint8_t* output, size_t* output_length);

#endif // MODE_0_H