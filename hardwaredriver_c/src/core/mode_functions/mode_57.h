#ifndef MODE_57_H
#define MODE_57_H

#include <stdbool.h>
#include <stdint.h>
#include "mode.h"
#include "mode_sweep.h"
#include "serial_interface.h"
#include "error_codes.h"

// Constants
#define MODE_57_READ_SIZE 1600
#define MODE_57_BLOCK_SIZE 16
#define MODE_57_MAX_COLLECT 1600
#define MODE_57_INIT_BYTES 32000
#define MODE_57_INIT_IGNORE_COUNT 25

// Mode 57 Raw implementation structure
typedef struct {
    ModeSweep base;
    bool is_first_run;
} Mode57Raw;

// Mode 57 Raw No Image implementation structure
typedef struct {
    ModeSweep base;
    bool is_first_run;
} Mode57RawNoImage;

// Constructor/Destructor for Mode57Raw
ErrorCode mode_57_raw_create(Mode57Raw** mode, SerialInterface*serial_interface);
void mode_57_raw_destroy(Mode57Raw* mode);
ErrorCode mode_57_raw_execute(Mode57Raw* mode, uint8_t* output, size_t* output_length);

// Constructor/Destructor for Mode57RawNoImage
ErrorCode mode_57_raw_no_image_create(Mode57RawNoImage** mode, SerialInterface*serial_interface);
void mode_57_raw_no_image_destroy(Mode57RawNoImage* mode);
ErrorCode mode_57_raw_no_image_execute(Mode57RawNoImage* mode, uint8_t* output, size_t* output_length);

#endif // MODE_57_H