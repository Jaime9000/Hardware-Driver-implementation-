#ifndef MODE_BASE_H
#define MODE_BASE_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "src/core/serial_interface.h"
#include "src/core/error_codes.h"
#include "src/gui/utils/process_manager.h"
#include "src/core/mode_functions/timing.h"

typedef struct ModeBase ModeBase;

typedef struct {
    int (*get_mode_number)(const ModeBase* mode);
    const uint8_t* (*get_emg_config)(const ModeBase* mode, size_t* length);
    ErrorCode (*execute_mode)(ModeBase* mode, uint8_t* output, size_t* output_length);
    ErrorCode (*execute_mode_not_connected)(ModeBase* mode, uint8_t* output, size_t* output_length);
    void (*stop)(ModeBase* mode);
    void (*destroy)(ModeBase* mode);
} ModeBaseVTable;

struct ModeBase {
    SerialInterface* interface;
    ProcessManager* process_manager;
    const ModeBaseVTable* vtable;
    void* impl;
    bool handshake_established;
    uint8_t device_byte;
};

// Constructor/Destructor
ErrorCode mode_base_create(ModeBase** mode, SerialInterface* interface, 
                         ProcessManager* process_manager, const ModeBaseVTable* vtable, void* impl);
void mode_base_destroy(ModeBase* mode);

// Core functionality
ErrorCode mode_base_handshake(ModeBase* mode);
ErrorCode mode_base_execute(ModeBase* mode, uint8_t* output, size_t* output_length, bool disconnected);
ErrorCode mode_base_flush_data(ModeBase* mode);
void mode_base_stop(ModeBase* mode);

// Getters
int mode_base_get_number(const ModeBase* mode);
const uint8_t* mode_base_get_emg_config(const ModeBase* mode, size_t* length);
uint8_t mode_base_get_device_byte(const ModeBase* mode);
bool mode_base_is_handshake_established(const ModeBase* mode);

// Add this declaration (no vtable changes needed)
bool mode_base_is_sweep(const ModeBase* mode);

#endif // MODE_BASE_H


/*
* Consider updating implementation to use macros for the required virtual functions
*
*
*
*
*/