#ifndef SCROLLABLE_FRAME_H
#define SCROLLABLE_FRAME_H

#include <tcl.h>
#include <tk.h>
#include "error_codes.h"
#include "commons.h"  // Added include

#define DEFAULT_CANVAS_WIDTH 450

typedef struct {
    Tcl_Interp* interp;
    char* container_name;
    char* canvas_path;
    char* frame_path;
    char* v_scrollbar_path;
    char* h_scrollbar_path;
} ScrollableFrame;

// Constructor/Destructor
ScrollableFrame* scrollable_frame_create(Tcl_Interp* interp, const char* container_name);
void scrollable_frame_destroy(ScrollableFrame* frame);

// Frame creation and configuration
ErrorCode scrollable_frame_initialize(ScrollableFrame* frame);
const char* scrollable_frame_get_path(ScrollableFrame* frame);

#endif // SCROLLABLE_FRAME_H