#ifndef MODE_SWEEP_H
#define MODE_SWEEP_H

#include "mode_base.h"
#include "error_codes.h"
#include "graph.h"
#include <stdint.h>

#define SCALING_FACTOR 57.2958
#define MODE_SWEEP_READ_SIZE 1600
#define K7_MODE_TYPE_PATH "C:\\K7\\current_mode_type"

typedef struct {
    Mode base;
    bool is_first_run;
    double front_angle;
    double side_angle;
    
    // UI Integration
    Graph* graph;
    bool show_tilt_window;
    bool show_sweep_graph;
    bool tilt_enabled;
} ModeSweep;

// Core functionality
ErrorCode mode_sweep_create(Mode** mode, bool show_tilt_window, bool show_sweep_graph);
ErrorCode mode_sweep_compute_angle(double ref, double axis1, double axis2, double* result);
ErrorCode mode_sweep_compute_tilt_data(const uint8_t* tilt_values, double* front_angle, double* side_angle);

// UI Integration
ErrorCode mode_sweep_start(ModeSweep* mode);
ErrorCode mode_sweep_stop(ModeSweep* mode);
ErrorCode mode_sweep_save_mode_type(bool show_sweep_graph);

#endif // MODE_SWEEP_H