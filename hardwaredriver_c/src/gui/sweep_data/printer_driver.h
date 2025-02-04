#ifndef PRINTER_DRIVER_H
#define PRINTER_DRIVER_H

// System headers
#include <windows.h>

// Project headers
#include "src/core/error_codes.h"

// Constants
#define SCALE_FACTOR 20  // Scale factor for image dimensions

/**
 * @brief Supported paper sizes for printing
 */
typedef enum {
    PAPER_LETTER = 1,        // US Letter (8.5" x 11")
    PAPER_LETTERSMALL = 2,   // US Letter Small
    PAPER_TABLOID = 3,       // Tabloid (11" x 17")
    PAPER_LEDGER = 4,        // Ledger (17" x 11")
    PAPER_LEGAL = 5,         // Legal (8.5" x 14")
    PAPER_STATEMENT = 6,     // Statement (5.5" x 8.5")
    PAPER_EXECUTIVE = 7,     // Executive (7.25" x 10.5")
    PAPER_A3 = 8,            // A3 (297mm x 420mm)
    PAPER_A4 = 9             // A4 (210mm x 297mm)
} PaperSize;

/**
 * @brief Page orientation options
 */
typedef enum {
    ORIENTATION_PORTRAIT = 1,   // Portrait orientation
    ORIENTATION_LANDSCAPE = 2   // Landscape orientation
} Orientation;

// Forward declaration
typedef struct PrintDocument PrintDocument;
// Constructor/Destructor
/**
 * @brief Creates a new print document instance
 * 
 * @param printer Name of printer to use, or NULL for default printer
 * @param paper_size Size of paper to print on
 * @param orientation Page orientation
 * @return PrintDocument* Pointer to new instance, or NULL on failure
 */
PrintDocument* print_document_create(const char* printer, 
                                   PaperSize paper_size, 
                                   Orientation orientation);

/**
 * @brief Destroys a print document instance and frees resources
 * 
 * @param doc Pointer to PrintDocument instance
 */
void print_document_destroy(PrintDocument* doc);
// Document operations
/**
 * @brief Begins a new print job
 * 
 * @param doc Pointer to PrintDocument instance
 * @param description Description of print job
 * @return ErrorCode ERROR_NONE on success, error code otherwise
 */
ErrorCode print_document_begin(PrintDocument* doc, const char* description);

/**
 * @brief Ends the current print job
 * 
 * @param doc Pointer to PrintDocument instance
 * @return ErrorCode ERROR_NONE on success, error code otherwise
 */
ErrorCode print_document_end(PrintDocument* doc);

/**
 * @brief Prints an image at the specified coordinates
 * 
 * @param doc Pointer to PrintDocument instance
 * @param x X coordinate for image placement
 * @param y Y coordinate for image placement
 * @param image_path Path to image file
 * @return ErrorCode ERROR_NONE on success, error code otherwise
 */
ErrorCode print_document_image(PrintDocument* doc, 
                             int x, 
                             int y, 
                             const char* image_path);

#endif // PRINTER_DRIVER_H