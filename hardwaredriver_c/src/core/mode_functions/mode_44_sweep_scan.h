#ifndef MODE_44_SWEEP_SCAN_H
#define MODE_44_SWEEP_SCAN_H

#include "mode.h"
#include "mode_sweep.h"
#include "serial_interface.h"
#include "error_codes.h"

// Mode 44 Sweep implementation structure
typedef struct {
    ModeSweep base;  // Inherit from ModeSweep
} Mode44Sweep;

// Constructor/Destructor
ErrorCode mode_44_sweep_create(Mode44Sweep** mode, SerialInterface* interface);
void mode_44_sweep_destroy(Mode44Sweep* mode);

#endif // MODE_44_SWEEP_SCAN_H