#pragma once
#include "termuxdc_event.h"
typedef void (*InputHandler)(termuxdc_event);

/**
 * @brief Status codes for all functions in the library.
 */
typedef enum {
    /// Operation was successful.
    TERMUX_DC_OK = 0,
    /// An error occurred in the system library. Check `errno` to get the exact error.
    TERMUX_DC_ERR_SYSTEM = 1,
    /// The connection to the plugin was lost. All further operations on this connection except destroy operations will fail with this error.
    TERMUX_DC_ERR_CONNECTION_LOST = 2,
    /// The Server the function tried to act on no longer exists.
    TERMUX_DC_ERR_SERVER_DESTROYED = 3,
    /// A server connection buffer could not be read or written.<br>
    /// This is usually an error on the server side and should be treated as fatal, and the connection should be closed.
    TERMUX_DC_ERR_MESSAGE = 4,
    /// There is not enough memory to complete an operation.
    TERMUX_DC_ERR_NOMEM = 5,
    /// An exception was thrown in the C++ part that could not be mapped to other errors.
    TERMUX_DC_ERR_EXCEPTION = 6,
    /// The View used was invalid or of a wrong type.
    TERMUX_DC_ERR_VIEW_INVALID = 7,
    /// The Android API level is too low for the function.
    TERMUX_DC_ERR_API_LEVEL = 8,
} termuxdc_state;
