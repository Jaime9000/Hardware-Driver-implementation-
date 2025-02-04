#ifndef PRINTER_H
#define PRINTER_H

// System headers
#include <windows.h>

// Project headers
#include "src/core/error_codes.h"

// Window dimensions
#define WINDOW_WIDTH 400
#define WINDOW_HEIGHT 300

// Maximum lengths
#define MAX_PRINTER_NAME 256
// Colors (optional, could be kept in .c file)
#define COLOR_LIGHTBLUE RGB(173, 216, 230)
// UI Colors
#define COLOR_BACKGROUND RGB(173, 216, 230)  // Light blue
#define COLOR_TEXT RGB(0, 0, 0)              // Black
// Function declarations
/**
 * @brief Opens a print dialog window for printer selection and configuration
 * 
 * @param filepath Path to the file to be printed
 * @return ErrorCode ERROR_NONE on success, error code otherwise
 */
ErrorCode open_print_dialog(const char* filepath);

/**
 * @brief Sets the specified printer as the system default printer
 * 
 * @param printer_name Name of the printer to use
 * @return ErrorCode ERROR_NONE on success, error code otherwise
 */
ErrorCode use_printer(const char* printer_name);

/**
 * @brief Executes the print job with current settings
 * 
 * @param filepath Path to the file to be printed
 * @param hwnd Handle to the print dialog window
 * @return ErrorCode ERROR_NONE on success, error code otherwise
 */
ErrorCode final_print(const char* filepath, HWND hwnd);

#endif // PRINTER_H