#ifndef PRINTER_DRIVER_H
#define PRINTER_DRIVER_H

#include <windows.h>
#include "error_codes.h"

#define SCALE_FACTOR 20

typedef enum {
    PAPER_LETTER = 1,
    PAPER_LETTERSMALL = 2,
    PAPER_TABLOID = 3,
    PAPER_LEDGER = 4,
    PAPER_LEGAL = 5,
    PAPER_STATEMENT = 6,
    PAPER_EXECUTIVE = 7,
    PAPER_A3 = 8,
    PAPER_A4 = 9
} PaperSize;

typedef enum {
    ORIENTATION_PORTRAIT = 1,
    ORIENTATION_LANDSCAPE = 2
} Orientation;

typedef struct PrintDocument PrintDocument;

// Constructor/Destructor
PrintDocument* print_document_create(const char* printer, PaperSize paper_size, Orientation orientation);
void print_document_destroy(PrintDocument* doc);

// Document operations
ErrorCode print_document_begin(PrintDocument* doc, const char* description);
ErrorCode print_document_end(PrintDocument* doc);
ErrorCode print_document_image(PrintDocument* doc, int x, int y, const char* image_path);

#endif // PRINTER_DRIVER_H