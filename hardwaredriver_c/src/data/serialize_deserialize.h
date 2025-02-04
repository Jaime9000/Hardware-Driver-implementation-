#ifndef SERIALIZE_DESERIALIZE_H
#define SERIALIZE_DESERIALIZE_H

// System headers
#include <stdint.h>
#include <stdbool.h>

// Project headers
#include "src/core/error_codes.h"

// Data structures for sweep measurements
typedef struct {
    double* timestamps;
    double* values;
    size_t count;
} SweepPoints;

typedef struct {
    SweepPoints sagittal;
    SweepPoints frontal;
    char run_type[32];
    char timestamp[64];
} SweepData;

// Data structures for application state
typedef struct {
    bool exit_thread;
    char event[64];
    char event_data[256];
    bool app_ready;
    char requested_playback_file[256];
} AppState;

// Data structure for patient info
typedef struct {
    char name[256];
    char path[1024];
} PatientInfo;

// Function declarations
ErrorCode sweep_data_serialize(const char* filepath, const SweepData* data);
ErrorCode sweep_data_deserialize(const char* filepath, SweepData** data);
void sweep_data_free(SweepData* data);

ErrorCode app_state_serialize(const char* filepath, const AppState* state);
ErrorCode app_state_deserialize(const char* filepath, AppState* state);

ErrorCode patient_info_serialize(const char* filepath, const PatientInfo* info);
ErrorCode patient_info_deserialize(const char* filepath, PatientInfo* info);

#endif // SERIALIZE_DESERIALIZE_H