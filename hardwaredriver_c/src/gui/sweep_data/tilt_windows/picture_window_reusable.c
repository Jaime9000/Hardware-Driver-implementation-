#include "picture_window_reusable.h"
#include <string.h>
#include <stdlib.h>
#include <windows.h>
#include "logger.h"

struct PictureWindowReusable {
    PictureWindowFunctions* base;
    Tcl_Interp* interp;
    char* window_path;
    char* canvas_path;
    char* frame_path;
    char* max_label_path;
    char* current_label_path;
    char* min_label_path;
    char* patient_name;
    bool is_frontal;
    bool is_running;
    bool window_hidden;
    ImageWindowOptions* image_options;
    CRITICAL_SECTION mutex;
};

static void draw_callback(ClientData client_data) {
    PictureWindowReusable* window = (PictureWindowReusable*)client_data;
    
    EnterCriticalSection(&window->mutex);
    if (!window->is_running) {
        LeaveCriticalSection(&window->mutex);
        return;
    }
    
    // Draw implementation here
    // ... (similar to Python's __draw method)
    
    // Schedule next draw
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "after 100 {DrawCallback}");
    Tcl_Eval(window->interp, cmd);
    
    LeaveCriticalSection(&window->mutex);
}

PictureWindowReusable* picture_window_reusable_create(/*...params...*/) {
    PictureWindowReusable* window = calloc(1, sizeof(PictureWindowReusable));
    if (!window) return NULL;
    
    InitializeCriticalSection(&window->mutex);
    // Initialize other members...
    
    // Create Tcl command for draw callback
    Tcl_CreateCommand(interp, "DrawCallback", draw_callback, (ClientData)window, NULL);
    
    return window;
}

ErrorCode picture_window_reusable_start(PictureWindowReusable* window) {
    EnterCriticalSection(&window->mutex);
    if (!window->is_running) {
        window->is_running = true;
        Tcl_Eval(window->interp, "after 100 {DrawCallback}");
    }
    LeaveCriticalSection(&window->mutex);
    return ERROR_NONE;
}