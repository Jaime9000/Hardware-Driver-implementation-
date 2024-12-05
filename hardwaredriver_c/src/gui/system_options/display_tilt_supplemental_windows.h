#ifndef DISPLAY_TILT_SUPPLEMENTAL_WINDOWS_H
#define DISPLAY_TILT_SUPPLEMENTAL_WINDOWS_H

#include <stdbool.h>
#include "error_codes.h"

#ifdef _WIN32
#define SUPPLEMENTAL_WINDOW_DISPLAY_SETTING_PATH "C:\\K7\\options_display"
#else
#define SUPPLEMENTAL_WINDOW_DISPLAY_SETTING_PATH "./options_display"
#endif

ErrorCode read_config_tilt_supplemental_windows(bool* show_windows);
ErrorCode show_config_tilt_supplemental_windows(void);
ErrorCode hide_config_tilt_supplemental_windows(void);

#endif // DISPLAY_TILT_SUPPLEMENTAL_WINDOWS_H