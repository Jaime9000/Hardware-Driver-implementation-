#ifndef MODE_44_H
#define MODE_44_H

#include "mode.h"
#include "mode_57.h"
#include "serial_interface.h"
#include "error_codes.h"

// Constants
#define MODE_44_REPEAT_COUNT 5
#define MODE_44_BLOCK_SIZE 8  // Size of block to repeat (first half of Mode57's 16-byte block)

// Mode 44 Raw implementation structure
typedef struct {
    Mode57Raw base;  // Inherit from Mode57Raw
} Mode44Raw;

// Mode 44 Raw No Image implementation structure
typedef struct {
    Mode57RawNoImage base;  // Inherit from Mode57RawNoImage
} Mode44RawNoImage;

// Constructor/Destructor for Mode44Raw
ErrorCode mode_44_raw_create(Mode44Raw** mode, SerialInterface* interface);
void mode_44_raw_destroy(Mode44Raw* mode);

// Constructor/Destructor for Mode44RawNoImage
ErrorCode mode_44_raw_no_image_create(Mode44RawNoImage** mode, SerialInterface* interface);
void mode_44_raw_no_image_destroy(Mode44RawNoImage* mode);

#endif // MODE_44_H