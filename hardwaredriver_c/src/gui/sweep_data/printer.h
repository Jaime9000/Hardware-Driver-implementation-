#ifndef PRINTER_H
#define PRINTER_H

#include <windows.h>
#include "error_codes.h"

// Window dimensions
#define WINDOW_WIDTH 400
#define WINDOW_HEIGHT 300

// Maximum lengths
#define MAX_PRINTER_NAME 256

// Colors (optional, could be kept in .c file)
#define COLOR_LIGHTBLUE RGB(173, 216, 230)

// Function declarations
ErrorCode open_print_dialog(const char* filepath);
ErrorCode use_printer(const char* printer_name);
ErrorCode final_print(const char* filepath, HWND hwnd);

#endif // PRINTER_H