#ifndef MODE_H
#define MODE_H

#include <stdint.h>
#include <stdbool.h>
#include "serial_interface.h"
#include "error_codes.h"

// Forward declarations
typedef struct Mode Mode;
typedef struct ModeVTable ModeVTable;

// Virtual table for mode operations
struct ModeVTable {
    int (*get_mode_number)(const Mode* mode);
    const uint8_t* (*get_emg_config)(const Mode* mode, size_t* length);
    ErrorCode (*execute)(Mode* mode);
    ErrorCode (*execute_not_connected)(Mode* mode);
    void (*destroy)(Mode* mode);
};

// Mode structure
struct Mode {
    SerialInterface* interface;
    const ModeVTable* vtable;
    void* impl;
    bool handshake_established;
    uint8_t device_byte;
};

// Mode functions
ErrorCode mode_init(Mode* mode, SerialInterface* interface, const ModeVTable* vtable, void* impl);
void mode_destroy(Mode* mode);
ErrorCode mode_handshake(Mode* mode);
ErrorCode mode_execute(Mode* mode, bool disconnected);
ErrorCode mode_flush_data(Mode* mode);
int mode_get_number(const Mode* mode);
const uint8_t* mode_get_emg_config(const Mode* mode, size_t* length);
uint8_t mode_get_device_byte(const Mode* mode);
bool mode_is_handshake_established(const Mode* mode);

#endif // MODE_H