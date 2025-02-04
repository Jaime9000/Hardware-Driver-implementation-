#include "src/gui/sweep_data/printer_driver.h"
#include "src/core/logger.h"

// System headers
#include <stdio.h>
#include <gdiplus.h>

#pragma comment(lib, "gdiplus.lib")

/**
 * @brief Internal structure of the PrintDocument
 */
struct PrintDocument {
    HDC printer_dc;              // Printer device context
    HANDLE printer_handle;       // Printer handle
    char* printer_name;          // Name of printer
    PaperSize paper_size;       // Selected paper size
    Orientation orientation;     // Page orientation
    int page;                   // Current page number
    DOCINFO doc_info;           // Document information
};

PrintDocument* print_document_create(const char* printer, 
                                   PaperSize paper_size, 
                                   Orientation orientation) {
    PrintDocument* doc = (PrintDocument*)calloc(1, sizeof(PrintDocument));
    if (!doc) {
        log_error("Failed to allocate PrintDocument");
        return NULL;
    }

    // Get printer name
    if (printer) {
        doc->printer_name = _strdup(printer);
    } else {
        DWORD needed = 0;
        GetDefaultPrinter(NULL, &needed);
        doc->printer_name = (char*)malloc(needed);
        if (!GetDefaultPrinter(doc->printer_name, &needed)) {
            log_error("Failed to get default printer");
            free(doc);
            return NULL;
        }
    }

    doc->paper_size = paper_size;
    doc->orientation = orientation;
    doc->page = 0;

    return doc;
}

void print_document_destroy(PrintDocument* doc) {
    if (!doc) return;

    if (doc->page != 0) {
        print_document_end(doc);
    }

    if (doc->printer_name) {
        free(doc->printer_name);
    }

    free(doc);
}

ErrorCode print_document_begin(PrintDocument* doc, const char* description) {
    if (!doc) return ERROR_INVALID_PARAMETER;

    // Open printer
    if (!OpenPrinter(doc->printer_name, &doc->printer_handle, NULL)) {
        log_error("Failed to open printer");
        return ERROR_PRINTER;
    }

    // Get printer settings
    PRINTER_INFO_2* printer_info = NULL;
    DWORD needed = 0;
    GetPrinter(doc->printer_handle, 2, NULL, 0, &needed);
    printer_info = (PRINTER_INFO_2*)malloc(needed);
    if (!GetPrinter(doc->printer_handle, 2, (LPBYTE)printer_info, needed, &needed)) {
        free(printer_info);
        ClosePrinter(doc->printer_handle);
        return ERROR_PRINTER;
    }

    DEVMODE* dev_mode = printer_info->pDevMode;
    dev_mode->dmPaperSize = doc->paper_size;
    dev_mode->dmOrientation = doc->orientation;
    dev_mode->dmFields |= DM_PAPERSIZE | DM_ORIENTATION;

    // Create DC
    doc->printer_dc = CreateDC("WINSPOOL", doc->printer_name, NULL, dev_mode);
    free(printer_info);

    if (!doc->printer_dc) {
        ClosePrinter(doc->printer_handle);
        return ERROR_PRINTER;
    }

    // Initialize document info
    ZeroMemory(&doc->doc_info, sizeof(DOCINFO));
    doc->doc_info.cbSize = sizeof(DOCINFO);
    doc->doc_info.lpszDocName = description ? description : "Print Job";

    // Start document
    if (StartDoc(doc->printer_dc, &doc->doc_info) <= 0) {
        DeleteDC(doc->printer_dc);
        ClosePrinter(doc->printer_handle);
        return ERROR_PRINTER;
    }

    if (StartPage(doc->printer_dc) <= 0) {
        EndDoc(doc->printer_dc);
        DeleteDC(doc->printer_dc);
        ClosePrinter(doc->printer_handle);
        return ERROR_PRINTER;
    }

    SetMapMode(doc->printer_dc, MM_TWIPS);
    doc->page = 1;

    return ERROR_NONE;
}

ErrorCode print_document_end(PrintDocument* doc) {
    if (!doc || doc->page == 0) return ERROR_INVALID_PARAMETER;

    EndPage(doc->printer_dc);
    EndDoc(doc->printer_dc);
    DeleteDC(doc->printer_dc);
    ClosePrinter(doc->printer_handle);

    doc->page = 0;
    return ERROR_NONE;
}

ErrorCode print_document_image(PrintDocument* doc, int x, int y, const char* image_path) {
    if (!doc || !image_path) return ERROR_INVALID_PARAMETER;

    if (doc->page == 0) {
        ErrorCode result = print_document_begin(doc, "Print Job");
        if (result != ERROR_NONE) return result;
    }

    // Initialize GDI+
    ULONG_PTR gdiplusToken;
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // Load and draw image
    WCHAR wide_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, image_path, -1, wide_path, MAX_PATH);
    
    GpImage* image = NULL;
    GpGraphics* graphics = NULL;
    ErrorCode result = ERROR_NONE;

    if (GdipLoadImageFromFile(wide_path, &image) != Ok) {
        result = ERROR_FILE_READ;
        goto cleanup;
    }

    if (GdipCreateFromHDC(doc->printer_dc, &graphics) != Ok) {
        result = ERROR_PRINTER;
        goto cleanup;
    }

    // Get image dimensions
    UINT width, height;
    GdipGetImageWidth(image, &width);
    GdipGetImageHeight(image, &height);

    // Draw image
    GpRectF rect = {(REAL)(x * SCALE_FACTOR), 
                    (REAL)(-y * SCALE_FACTOR), 
                    (REAL)(width * SCALE_FACTOR), 
                    (REAL)(-height * SCALE_FACTOR)};
    
    if (GdipDrawImageRect(graphics, image, rect.X, rect.Y, rect.Width, rect.Height) != Ok) {
        result = ERROR_PRINTER;
    }

cleanup:
    if (graphics) GdipDeleteGraphics(graphics);
    if (image) GdipDisposeImage(image);
    GdiplusShutdown(gdiplusToken);

    return result;
}