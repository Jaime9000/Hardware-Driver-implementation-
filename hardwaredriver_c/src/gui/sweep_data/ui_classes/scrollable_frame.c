#include "scrollable_frame.h"
#include <string.h>
#include <stdlib.h>
#include "logger.h"

static void on_configure(ClientData client_data, Tcl_Interp* interp, int argc, const char* argv[]) {
    ScrollableFrame* frame = (ScrollableFrame*)client_data;
    
    // Update canvas scroll region to match frame size
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "%s configure -scrollregion [%s bbox all]",
             frame->canvas_path, frame->canvas_path);
    Tcl_Eval(interp, cmd);
}

static void on_mousewheel(ClientData client_data, Tcl_Interp* interp, int argc, const char* argv[]) {
    ScrollableFrame* frame = (ScrollableFrame*)client_data;
    
    if (argc < 2) return;
    int delta = atoi(argv[1]);
    
    // Scroll canvas based on mousewheel delta
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "%s yview scroll %d units",
             frame->canvas_path, (int)(-delta / 120));
    Tcl_Eval(interp, cmd);
}

ScrollableFrame* scrollable_frame_create(Tcl_Interp* interp, const char* container_name) {
    ScrollableFrame* frame = calloc(1, sizeof(ScrollableFrame));
    if (!frame) {
        log_error("Failed to allocate ScrollableFrame");
        return NULL;
    }

    frame->interp = interp;
    frame->container_name = strdup(container_name);
    
    // Generate unique paths
    char buf[128];
    snprintf(buf, sizeof(buf), "%s.canvas", container_name);
    frame->canvas_path = strdup(buf);
    
    snprintf(buf, sizeof(buf), "%s.vscroll", container_name);
    frame->v_scrollbar_path = strdup(buf);
    
    snprintf(buf, sizeof(buf), "%s.hscroll", container_name);
    frame->h_scrollbar_path = strdup(buf);
    
    snprintf(buf, sizeof(buf), "%s.frame", frame->canvas_path);
    frame->frame_path = strdup(buf);

    return frame;
}

ErrorCode scrollable_frame_initialize(ScrollableFrame* frame) {
    if (!frame || !frame->interp) return ERROR_INVALID_PARAMETER;

    // Create canvas
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "canvas %s -width %d",
             frame->canvas_path, DEFAULT_CANVAS_WIDTH);
    if (Tcl_Eval(frame->interp, cmd) != TCL_OK) {
        log_error("Failed to create canvas");
        return ERROR_TCL_EVAL;
    }

    // Create scrollbars
    snprintf(cmd, sizeof(cmd),
             "scrollbar %s -orient vertical -command {%s yview}",
             frame->v_scrollbar_path, frame->canvas_path);
    if (Tcl_Eval(frame->interp, cmd) != TCL_OK) return ERROR_TCL_EVAL;

    snprintf(cmd, sizeof(cmd),
             "scrollbar %s -orient horizontal -command {%s xview}",
             frame->h_scrollbar_path, frame->canvas_path);
    if (Tcl_Eval(frame->interp, cmd) != TCL_OK) return ERROR_TCL_EVAL;

    // Create inner frame
    snprintf(cmd, sizeof(cmd), "frame %s", frame->frame_path);
    if (Tcl_Eval(frame->interp, cmd) != TCL_OK) return ERROR_TCL_EVAL;

    // Create window in canvas
    snprintf(cmd, sizeof(cmd),
             "%s create window 0 0 -window %s -anchor nw",
             frame->canvas_path, frame->frame_path);
    if (Tcl_Eval(frame->interp, cmd) != TCL_OK) return ERROR_TCL_EVAL;

    // Configure scrolling
    snprintf(cmd, sizeof(cmd),
             "%s configure -yscrollcommand {%s set} -xscrollcommand {%s set}",
             frame->canvas_path, frame->v_scrollbar_path, frame->h_scrollbar_path);
    if (Tcl_Eval(frame->interp, cmd) != TCL_OK) return ERROR_TCL_EVAL;

    // Pack scrollbars and canvas
    Tcl_Eval(frame->interp, "pack propagate [winfo parent $frame] true");
    snprintf(cmd, sizeof(cmd),
             "pack %s -side bottom -fill x; "
             "pack %s -side right -fill y; "
             "pack %s -side left -fill both -expand true",
             frame->h_scrollbar_path, frame->v_scrollbar_path, frame->canvas_path);
    if (Tcl_Eval(frame->interp, cmd) != TCL_OK) return ERROR_TCL_EVAL;

    // Bind events
    Tcl_CreateCommand(frame->interp, "OnConfigure", on_configure, (ClientData)frame, NULL);
    Tcl_CreateCommand(frame->interp, "OnMousewheel", on_mousewheel, (ClientData)frame, NULL);
    
    // Updated to use SCROLLABLE_CLASS_NAME from commons.h
    snprintf(cmd, sizeof(cmd),
             "bind %s <Configure> {OnConfigure %%D}; "
             "bindtags %s {%s}; "
             "bind %s <MouseWheel> {OnMousewheel %%D}",
             frame->frame_path,
             frame->canvas_path, SCROLLABLE_CLASS_NAME,
             SCROLLABLE_CLASS_NAME);
    if (Tcl_Eval(frame->interp, cmd) != TCL_OK) return ERROR_TCL_EVAL;

    return ERROR_NONE;
}

const char* scrollable_frame_get_path(ScrollableFrame* frame) {
    return frame ? frame->frame_path : NULL;
}

void scrollable_frame_destroy(ScrollableFrame* frame) {
    if (!frame) return;
    
    free(frame->container_name);
    free(frame->canvas_path);
    free(frame->frame_path);
    free(frame->v_scrollbar_path);
    free(frame->h_scrollbar_path);
    free(frame);
}