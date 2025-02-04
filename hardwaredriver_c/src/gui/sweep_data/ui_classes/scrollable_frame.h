#ifndef SCROLLABLE_FRAME_H
#define SCROLLABLE_FRAME_H

// System headers
#include <tcl.h>
#include <tk.h>

// Project headers
#include "src/core/error_codes.h"
#include "src/gui/sweep_data/ui_classes/commons.h"

// Constants
#define DEFAULT_CANVAS_WIDTH 450
#define MAX_PATH_LENGTH 128
#define MAX_CMD_LENGTH 512

/**
 * @brief Structure representing a scrollable frame widget
 */
typedef struct {
    Tcl_Interp* interp;          // Tcl interpreter
    char* container_name;        // Name of the container widget
    char* canvas_path;          // Path to the canvas widget
    char* frame_path;           // Path to the inner frame
    char* v_scrollbar_path;     // Path to vertical scrollbar
    char* h_scrollbar_path;     // Path to horizontal scrollbar
} ScrollableFrame;

/**
 * @brief Creates a new scrollable frame instance
 * 
 * @param interp Tcl interpreter
 * @param container_name Name of the container widget
 * @return ScrollableFrame* Pointer to new instance, or NULL on failure
 */
ScrollableFrame* scrollable_frame_create(Tcl_Interp* interp, const char* container_name);

/**
 * @brief Destroys a scrollable frame instance and frees all resources
 * 
 * @param frame Pointer to ScrollableFrame instance
 */
void scrollable_frame_destroy(ScrollableFrame* frame);

/**
 * @brief Initializes the scrollable frame widgets and event bindings
 * 
 * Creates and configures:
 * - Canvas widget
 * - Scrollbars (vertical and horizontal)
 * - Inner frame
 * - Event bindings for scrolling and resizing
 * 
 * @param frame Pointer to ScrollableFrame instance
 * @return ErrorCode ERROR_NONE on success, error code otherwise
 */
ErrorCode scrollable_frame_initialize(ScrollableFrame* frame);

/**
 * @brief Gets the path to the inner frame widget
 * 
 * @param frame Pointer to ScrollableFrame instance
 * @return const char* Path to inner frame, or NULL if frame is NULL
 */
const char* scrollable_frame_get_path(ScrollableFrame* frame);

#endif // SCROLLABLE_FRAME_H