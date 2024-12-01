// mode_sweep.h
#ifndef MODE_SWEEP_H
#define MODE_SWEEP_H

#include "mode_base.h"
#include "error_codes.h"
#include <stdint.h>

#define SCALING_FACTOR 57.2958
#define MODE_SWEEP_READ_SIZE 1600

typedef struct {
    Mode base;
    bool is_first_run;
    double front_angle;
    double side_angle;
} ModeSweep;

ErrorCode mode_sweep_create(Mode** mode);
ErrorCode mode_sweep_compute_angle(double ref, double axis1, double axis2, double* result);
ErrorCode mode_sweep_compute_tilt_data(const uint8_t* tilt_values, double* front_angle, double* side_angle);

#endif // MODE_SWEEP_H