#ifndef MODE_42_H
#define MODE_42_H

#include <stdbool.h>
#include <stdint.h>
#include "mode.h"
#include "serial_interface.h"
#include "byte_sync.h"
#include "error_codes.h"

// Constants
#define MODE_42_BLOCK_SIZE 24
#define MODE_42_CMS_SIZE 8
#define MODE_42_EMG_SIZE 16
#define MODE_42_READ_SIZE 320
#define MODE_42_INIT_THRESHOLD 32000
#define MODE_42_INIT_IGNORE_COUNT 50

// Equipment byte mode
typedef struct {
    Mode base;
    uint8_t device_byte;
} EquipmentByte;

ErrorCode equipment_byte_create(EquipmentByte** mode, SerialInterface* interface);
void equipment_byte_destroy(EquipmentByte* mode);

// Notch filter types
typedef enum {
    NOTCH_FILTER_NONE = 0,  // Regular Mode42Raw
    NOTCH_FILTER_P = 'p',
    NOTCH_FILTER_Q = 'q',
    NOTCH_FILTER_R = 'r',
    NOTCH_FILTER_S = 's',
    NOTCH_FILTER_T = 't',
    NOTCH_FILTER_U = 'u',
    NOTCH_FILTER_V = 'v',
    NOTCH_FILTER_W = 'w'
} NotchFilterType;

// Raw mode variants
typedef struct {
    Mode base;
    bool is_first_run;
    NotchFilterType filter_type;
} Mode42Raw;

// Base Raw mode
ErrorCode mode_42_raw_create(Mode42Raw** mode, SerialInterface* interface);
void mode_42_raw_destroy(Mode42Raw* mode);

// EMG Lead Status mode
typedef struct {
    Mode base;
} GetEMGLeadStatus;

ErrorCode emg_lead_status_create(GetEMGLeadStatus** mode, SerialInterface* interface);
void emg_lead_status_destroy(GetEMGLeadStatus* mode);

// Create notch filter variant
ErrorCode mode_42_raw_notch_create(Mode42Raw** mode, SerialInterface* interface, NotchFilterType filter_type);

#endif // MODE_42_H