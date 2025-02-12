#ifndef MYOTRONICS_SERIAL_INTERFACE_H
#define MYOTRONICS_SERIAL_INTERFACE_H

// System headers
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <stdbool.h>

// Project headers
#include "src/core/commands.h"
#include "src/core/error_codes.h"
#include "src/core/logger.h"
#include "src/core/main/config.h"
#include "src/core/mode_functions/mode_manager.h"  // needed for mode_manager_execute_command

// Serial port configuration constants
#define SLOW_BAUD_RATE 115200
#define FAST_BAUD_RATE 230400
#define DEFAULT_TIMEOUT 5000  // 5 seconds
#define MAX_BUFFER_SIZE 32000
#define MAX_STATUS_STRING_LENGTH 32

// Handshake constants
#define HANDSHAKE_STRING_50_HZ "K7-MYO5"
#define HANDSHAKE_STRING_60_HZ "K7-MYO6"
#define MAX_HANDSHAKE_ATTEMPTS 10
#define HANDSHAKE_RESPONSE_PREFIX "K7-MYO Ver"
#define MAX_VERSION_STRING_LENGTH 32

// Serial interface structure
typedef struct {
    HANDLE handle;                  // Windows handle for the serial port
    CRITICAL_SECTION mutex;         // Thread synchronization
    Config* config;                 // Pointer to configuration
    bool is_connected;             // Connection status
    char* port_name;               // Serial port name
    DWORD baud_rate;               // Current baud rate
    bool handshake_established;    // Handshake status
    Logger* logger;                // System logger
} SerialInterface;

// Constructor/Destructor
/**
 * Creates a new serial interface instance
 * @param config Pointer to system configuration
 * @return Pointer to new SerialInterface or NULL on failure
 */
SerialInterface* serial_interface_create(Config* config);

/**
 * Destroys a serial interface instance and frees resources
 * @param interface Pointer to SerialInterface to destroy
 */
void serial_interface_destroy(SerialInterface* interface);

// Basic serial operations
/**
 * Opens the serial port connection
 * @param interface Pointer to SerialInterface
 * @return ERROR_NONE on success, error code otherwise
 */
ErrorCode serial_interface_open(SerialInterface* interface);

/**
 * Closes the serial port connection
 * @param interface Pointer to SerialInterface
 * @return ERROR_NONE on success, error code otherwise
 */
ErrorCode serial_interface_close(SerialInterface* interface);

/**
 * Writes data to the serial port
 * @param interface Pointer to SerialInterface
 * @param data Data buffer to write
 * @param length Length of data to write
 * @return ERROR_NONE on success, error code otherwise
 */
ErrorCode serial_interface_write_data(SerialInterface* interface, const unsigned char* data, size_t length);

/**
 * Reads data from the serial port
 * @param interface Pointer to SerialInterface
 * @param buffer Buffer to store read data
 * @param bytes_read Number of bytes actually read
 * @param max_length Maximum number of bytes to read
 * @return ERROR_NONE on success, error code otherwise
 */
ErrorCode serial_interface_read_data(SerialInterface* interface, unsigned char* buffer, size_t* bytes_read, size_t max_length);

/**
 * Flushes the serial port buffers
 * @param interface Pointer to SerialInterface
 * @return ERROR_NONE on success, error code otherwise
 */
ErrorCode serial_interface_flush(SerialInterface* interface);

// USB Control functions
/**
 * Enables USB control signals (RTS)
 * @param interface Pointer to SerialInterface
 * @return ERROR_NONE on success, error code otherwise
 */
ErrorCode serial_interface_usb_control_on(SerialInterface* interface);

/**
 * Disables USB control signals (RTS)
 * @param interface Pointer to SerialInterface
 * @return ERROR_NONE on success, error code otherwise
 */
ErrorCode serial_interface_usb_control_off(SerialInterface* interface);

/**
 * Enables USB data signals (DTR)
 * @param interface Pointer to SerialInterface
 * @return ERROR_NONE on success, error code otherwise
 */
ErrorCode serial_interface_usb_data_on(SerialInterface* interface);

/**
 * Disables USB data signals (DTR)
 * @param interface Pointer to SerialInterface
 * @return ERROR_NONE on success, error code otherwise
 */
ErrorCode serial_interface_usb_data_off(SerialInterface* interface);

// Hardware control functions
/**
 * Resets the hardware and performs handshake
 * @param interface Pointer to SerialInterface
 * @param is_60hz True for 60Hz mode, false for 50Hz mode
 * @return ERROR_NONE on success, error code otherwise
 */
ErrorCode serial_interface_reset_hardware(SerialInterface* interface, bool is_60hz);

/**
 * Gets the control status of all serial lines
 * @param interface Pointer to SerialInterface
 * @param status_string Buffer to store status string
 * @param as_json True to return JSON format, false for string format
 * @return ERROR_NONE on success, error code otherwise
 */
ErrorCode serial_interface_control_statuses(SerialInterface* interface, char* status_string, bool as_json);

/**
 * Gets the EMG version string
 * @param interface Pointer to SerialInterface
 * @param version_string Buffer to store version string
 * @param max_length Maximum length of version string buffer
 * @return ERROR_NONE on success, error code otherwise
 */
ErrorCode serial_interface_get_emg_version(SerialInterface* interface, char* version_string, size_t max_length);

/**
 * Checks the device connection status
 * @param interface Pointer to SerialInterface
 * @return ERROR_NONE if connected, error code otherwise
 */
ErrorCode serial_interface_check_connection(SerialInterface* interface);

/**
 * Gets the equipment byte from the device
 * @param interface Pointer to SerialInterface
 * @param equipment_byte Buffer to store equipment byte
 * @return ERROR_NONE on success, error code otherwise
 */
ErrorCode serial_interface_get_equipment_byte(SerialInterface* interface, unsigned char* equipment_byte);

/**
 * Gets the current frequency configuration
 * @param interface Pointer to SerialInterface
 * @param frequency Pointer to store the frequency value
 * @return ERROR_NONE on success, error code otherwise
 */
ErrorCode serial_interface_get_current_freq_config(SerialInterface* interface, int* frequency);

/**
 * Sets the current frequency configuration
 * @param interface Pointer to SerialInterface
 * @param frequency Frequency value to set
 * @return ERROR_NONE on success, error code otherwise
 */
ErrorCode serial_interface_set_current_freq_config(SerialInterface* interface, int frequency);

// Buffer management
/**
 * Resets the input and output buffers
 * @param interface Pointer to SerialInterface
 * @return ERROR_NONE on success, error code otherwise
 */
ErrorCode serial_interface_reset_buffers(SerialInterface* interface);

// Handshake functions
/**
 * Performs the handshake sequence with the device
 * @param interface Pointer to SerialInterface
 * @param handshake_string Handshake string to use
 * @return ERROR_NONE on success, error code otherwise
 */
ErrorCode serial_interface_perform_handshake(SerialInterface* interface, const char* handshake_string);

/**
 * Checks if handshake has been established
 * @param interface Pointer to SerialInterface
 * @return true if handshake is established, false otherwise
 */
bool serial_interface_is_handshake_established(SerialInterface* interface);

// Status retrieval functions
/**
 * Gets the current RTS (Request to Send) status
 * @param interface Pointer to SerialInterface
 * @return true if RTS is set, false otherwise
 */
bool serial_interface_get_rts_status(SerialInterface* interface);

/**
 * Gets the current DTR (Data Terminal Ready) status
 * @param interface Pointer to SerialInterface
 * @return true if DTR is set, false otherwise
 */
bool serial_interface_get_dtr_status(SerialInterface* interface);

/**
 * Gets the current CTS (Clear to Send) status
 * @param interface Pointer to SerialInterface
 * @return true if CTS is set, false otherwise
 */
bool serial_interface_get_cts_status(SerialInterface* interface);

/**
 * Gets the current DSR (Data Set Ready) status
 * @param interface Pointer to SerialInterface
 * @return true if DSR is set, false otherwise
 */
bool serial_interface_get_dsr_status(SerialInterface* interface);

/**
 * Gets the current CD (Carrier Detect) status
 * @param interface Pointer to SerialInterface
 * @return true if CD is set, false otherwise
 */
bool serial_interface_get_cd_status(SerialInterface* interface);

/**
 * Gets the current RI (Ring Indicator) status
 * @param interface Pointer to SerialInterface
 * @return true if RI is set, false otherwise
 */
bool serial_interface_get_ri_status(SerialInterface* interface);

// Command management functions
/**
 * @brief Executes a command through the serial interface
 * @param interface Pointer to SerialInterface
 * @param command Command buffer to execute
 * @param command_size Size of command buffer
 * @param return_size Expected size of return data
 * @param mode_manager Pointer to ModeManager instance
 * @return ERROR_NONE on success, error code otherwise
 */
ErrorCode serial_interface_execute_command(SerialInterface* interface, 
                                        const unsigned char* command,
                                        size_t command_size,
                                        size_t return_size,
                                        ModeManager* mode_manager);

/**
 * @brief Registers a command handler with the serial interface
 * @param interface Pointer to SerialInterface
 * @param command Command to register
 * @param execute_func Function to handle the command
 * @return ERROR_NONE on success, error code otherwise
 */
ErrorCode serial_interface_register_command(SerialInterface* interface, 
                                         IOCommand command,
                                         ModeExecuteFunc execute_func);

#endif // MYOTRONICS_SERIAL_INTERFACE_H