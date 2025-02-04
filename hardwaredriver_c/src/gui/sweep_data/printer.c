#include "src/gui/sweep_data/printer.h"
#include "src/gui/sweep_data/printer_driver.h"
#include "src/core/logger.h"

// System headers
#include <commctrl.h>
#include <gdiplus.h>

// Window class name
//static const wchar_t* PRINTER_DIALOG_CLASS = L"PrinterDialogClass";

// Window constants
#define WINDOW_TITLE L"Printer Configuration"
#define WINDOW_CLASS L"PrinterDialogClass"
#define BUTTON_TEXT "Print"

// UI dimensions
#define LISTBOX_MARGIN 10
#define LISTBOX_WIDTH (WINDOW_WIDTH - 2 * LISTBOX_MARGIN)
#define LISTBOX_HEIGHT (WINDOW_HEIGHT - 80)
#define BUTTON_WIDTH 80
#define BUTTON_HEIGHT 25
#define BUTTON_Y (WINDOW_HEIGHT - 60)

// Global state
static struct {
    char filepath[MAX_PATH];
    HWND listbox;
    HWND print_button;
    HBRUSH background_brush;
    ULONG_PTR gdiplusToken;
} g_state = {0};

// Forward declarations
static LRESULT CALLBACK PrinterDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static void populate_printer_list(HWND listbox);
static ErrorCode initialize_gdi_plus(void);
static ErrorCode create_window_class(void);
static ErrorCode create_main_window(void);
static ErrorCode create_window_controls(HWND hwnd);

/**
 * @brief Populates the printer list with available printers
 */
static void populate_printer_list(HWND listbox) {
    DWORD flags = PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS;
    DWORD needed = 0;
    DWORD returned = 0;
    
    // Get required buffer size
    EnumPrinters(flags, NULL, 2, NULL, 0, &needed, &returned);
    if (needed == 0) {
        log_error("No printers found");
        return;
    }

    // Allocate buffer for printer info
    PRINTER_INFO_2* printer_info = (PRINTER_INFO_2*)malloc(needed);
    if (!printer_info) {
        log_error("Failed to allocate memory for printer info");
        return;
    }

    // Get printer information
    if (EnumPrinters(flags, NULL, 2, (LPBYTE)printer_info, needed, &needed, &returned)) {
        for (DWORD i = 0; i < returned; i++) {
            char* printer_name = printer_info[i].pPrinterName;
            // Remove everything after comma in printer name
            char* comma = strchr(printer_name, ',');
            if (comma) *comma = '\0';
            SendMessageA(listbox, LB_ADDSTRING, 0, (LPARAM)printer_name);
        }
    } else {
        log_error("Failed to enumerate printers");
    }

    free(printer_info);
}

LRESULT CALLBACK PrinterDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            // Create lightblue background brush
            g_state.background_brush = CreateSolidBrush(RGB(173, 216, 230));
            SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)g_state.background_brush);

            // Create listbox with scrollbar
            g_state.listbox = CreateWindowEx(
                WS_EX_CLIENTEDGE, "LISTBOX", "",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_SINGLE,
                LISTBOX_MARGIN, LISTBOX_MARGIN, LISTBOX_WIDTH, LISTBOX_HEIGHT,
                hwnd, NULL, GetModuleHandle(NULL), NULL
            );

            // Set listbox selection background color
            SendMessage(g_state.listbox, LB_SETSEL, TRUE, -1);

            // Create print button
            g_state.print_button = CreateWindow(
                "BUTTON", BUTTON_TEXT,
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                (WINDOW_WIDTH - BUTTON_WIDTH) / 2, BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT,  // Center button
                hwnd, (HMENU)1, GetModuleHandle(NULL), NULL
            );

            populate_printer_list(g_state.listbox);
            return 0;
        }

        case WM_COMMAND: {
            if (HIWORD(wParam) == LBN_SELCHANGE) {
                char printer_name[MAX_PRINTER_NAME];
                int index = SendMessage(g_state.listbox, LB_GETCURSEL, 0, 0);
                SendMessage(g_state.listbox, LB_GETTEXT, index, (LPARAM)printer_name);
                use_printer(printer_name);
            }
            else if (LOWORD(wParam) == 1) { // Print button
                final_print(g_state.filepath, hwnd);
            }
            return 0;
        }

        case WM_CTLCOLORLISTBOX: {
            // Set listbox colors
            SetBkColor((HDC)wParam, RGB(173, 216, 230));  // lightblue
            SetTextColor((HDC)wParam, RGB(0, 0, 0));      // black text
            return (LRESULT)g_state.background_brush;
        }

        case WM_DESTROY:
            if (g_state.background_brush) {
                DeleteObject(g_state.background_brush);
            }
            if (g_state.gdiplusToken) {
                GdiplusShutdown(g_state.gdiplusToken);
            }
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

ErrorCode open_print_dialog(const char* filepath) {
    strncpy(g_state.filepath, filepath, MAX_PATH - 1);
    g_state.filepath[MAX_PATH - 1] = '\0';

    // Initialize GDI+
    GdiplusStartupInput gdiplusStartupInput;
    gdiplusStartupInput.GdiplusVersion = 1;
    gdiplusStartupInput.DebugEventCallback = NULL;
    gdiplusStartupInput.SuppressBackgroundThread = FALSE;
    gdiplusStartupInput.SuppressExternalCodecs = FALSE;
    
    if (GdiplusStartup(&g_state.gdiplusToken, &gdiplusStartupInput, NULL) != Ok) {
        log_error("Failed to initialize GDI+");
        return ERROR_INVALID_STATE;
    }

    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = PrinterDialogProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = WINDOW_CLASS;
    
    if (!RegisterClassEx(&wc)) {
        log_error("Failed to register window class");
        return ERROR_INVALID_STATE;
    }

    HWND hwnd = CreateWindowEx(
        0,
        WINDOW_CLASS,
        WINDOW_TITLE,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        NULL, NULL, GetModuleHandle(NULL), NULL
    );

    if (!hwnd) {
        log_error("Failed to create window");
        return ERROR_INVALID_STATE;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return ERROR_NONE;
}

ErrorCode use_printer(const char* printer_name) {
    if (!SetDefaultPrinter(printer_name)) {
        log_error("Failed to set default printer");
        return ERROR_INVALID_STATE;
    }
    return ERROR_NONE;
}

ErrorCode final_print(const char* filepath, HWND hwnd) {
    char current_printer[MAX_PRINTER_NAME];
    DWORD size = MAX_PRINTER_NAME;
    
    if (!GetDefaultPrinter(current_printer, &size)) {
        log_error("Failed to get default printer");
        return ERROR_INVALID_STATE;
    }

    // Get image dimensions using GDI+
    WCHAR wfilepath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, filepath, -1, wfilepath, MAX_PATH);
    
    GpBitmap* bitmap = NULL;
    GdipLoadImageFromFile(wfilepath, &bitmap);
    if (!bitmap) {
        log_error("Failed to load image");
        return ERROR_INVALID_STATE;
    }

    UINT width, height;
    GdipGetImageWidth(bitmap, &width);
    GdipGetImageHeight(bitmap, &height);
    GdipDisposeImage(bitmap);

    PrintDocument* doc = print_document_create(
        current_printer, 
        PAPER_A4, 
        ORIENTATION_LANDSCAPE
    );

    if (!doc) {
        log_error("Failed to create print document");
        return ERROR_INVALID_STATE;
    }

    ErrorCode result = print_document_image(doc, 0, 0, filepath);
    
    if (result == ERROR_NONE) {
        result = print_document_end(doc);
    }

    print_document_destroy(doc);
    DestroyWindow(hwnd);
    
    return result;
}