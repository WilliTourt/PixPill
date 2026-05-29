/*******************************************************************************
 * @file    ElegantDebug.cpp
 * @version 1.3
 * @brief   C++ implementation for ANSI-colored debug logging on STM32.
 *
 * Implements the `ElegantDebug` class declared in `Src-CPP/debug.h`. The class
 * provides formatted logging helpers that send output over a HAL UART
 * interface. Optional timestamps (via `HAL_GetTick()`) and ANSI color
 * prefixes are supported for terminals that accept escape sequences.
 *
 * Usage: Construct `ElegantDebug` with a `UART_HandleTypeDef*` then call `log(),`
 * `info(),` `error()`, etc. See README for examples and integration notes.
 *
 * @author:    WilliTourt <willitourt@foxmail.com>
 * @date:      2026-03-01
 * 
 * @changelog:
 * - (See header file)
 ******************************************************************************/

#include "ElegantDebug.h"

#if __cplusplus < 202002L
    ElegantDebug::ElegantDebug(bool enable_timestamp, bool enable_color) :
        _timestamp_enabled(enable_timestamp), _color_enabled(enable_color) {}

    ElegantDebug::ElegantDebug(UART_HandleTypeDef *huart, bool enable_timestamp, bool enable_color) :
        _huart(huart), _timestamp_enabled(enable_timestamp), _color_enabled(enable_color) {}
#else
    ElegantDebug::ElegantDebug(bool enable_timestamp, bool enable_color,
                               bool enable_filename_line) :
                               _timestamp_enabled(enable_timestamp),
                               _color_enabled(enable_color),
                               _filename_line_enabled(enable_filename_line) {}

    ElegantDebug::ElegantDebug(UART_HandleTypeDef *huart, bool enable_timestamp,
                               bool enable_color, bool enable_filename_line) :
                               _huart(huart),
                               _timestamp_enabled(enable_timestamp),
                               _color_enabled(enable_color),
                               _filename_line_enabled(enable_filename_line) {}
#endif

void ElegantDebug::_send(const char* text) {
    char out[DEBUG_BUFFER_LEN * 2];
    size_t pos = 0;
    if (_timestamp_enabled) { // Prefix timestamp [hh:mm:ss.mmm] using HAL_GetTick()
        uint32_t ms = HAL_GetTick();
        uint32_t s = ms / 1000U;
        uint32_t hours = s / 3600U;
        uint32_t minutes = (s % 3600U) / 60U;
        uint32_t seconds = s % 60U;
        pos = snprintf(out, sizeof(out), "[%02lu:%02lu:%02lu.%03lu] ",
                       (unsigned long)hours, (unsigned long)minutes,
                       (unsigned long)seconds, (unsigned long)(ms % 1000U));
        if (pos >= sizeof(out)) pos = 0; // guard
    }
    // append text safely
    strncpy(out + pos, text, sizeof(out) - pos - 1);
    out[sizeof(out) - 1] = '\0';

    #if (USB_AS_DEBUG_PORT == 1)
        CDC_Transmit_FS((uint8_t*)out, (uint16_t)strlen(out));
    #else
        HAL_UART_Transmit(_huart, (uint8_t*)out, (uint16_t)strlen(out), HAL_MAX_DELAY);
    #endif
}



void ElegantDebug::log(const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);
    _send(msg);
}

// #if __cplusplus >= 202002L
// void ElegantDebug::logWithType(const char* type, const char* format, std::source_location loc, ...) {
//     char msg[DEBUG_BUFFER_LEN];
//     va_list args;
//     va_start(args, loc);
//     vsnprintf(msg, sizeof(msg), format, args);
//     va_end(args);

//     char combined[DEBUG_BUFFER_LEN + 96];
//     // if (_filename_line_enabled) {
//     //     snprintf(combined, sizeof(combined), "\033[1m[%s]\033[0m [%s:%ld] %s", type, loc.file_name(), loc.line(), msg);
//     // } else {
//         snprintf(combined, sizeof(combined), "\033[1m[%s]\033[0m %s", type, msg);
//     // }
//     _send(combined);
// }
// #else
void ElegantDebug::logWithType(const char* type, const char* style, const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

    char combined[DEBUG_BUFFER_LEN + 32];
    snprintf(combined, sizeof(combined), "\033[1m%s[%s]\033[0m %s", style, type, msg);
    _send(combined);
}
// #endif

#if __cplusplus >= 202002L
void ElegantDebug::error(const char* format, std::source_location loc, ...) {
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, loc);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

    const char* prefix = _color_enabled ? ERROR_TYPE : ERROR_TYPE_PLAIN;
    char combined[DEBUG_BUFFER_LEN + 96];

    if (_filename_line_enabled) {
        snprintf(combined, sizeof(combined), "%s[%s:%ld] %s", prefix, loc.file_name(), loc.line(), msg);
    } else {
        snprintf(combined, sizeof(combined), "%s%s", prefix, msg);
    }
    _send(combined);
}
#else
void ElegantDebug::error(const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

    const char* prefix = _color_enabled ? ERROR_TYPE : ERROR_TYPE_PLAIN;
    char combined[DEBUG_BUFFER_LEN + 32];

    snprintf(combined, sizeof(combined), "%s%s", prefix, msg);
    _send(combined);
}
#endif

#if __cplusplus >= 202002L
void ElegantDebug::warning(const char* format, std::source_location loc, ...) {
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, loc);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

    const char* prefix = _color_enabled ? WARNING_TYPE : WARNING_TYPE_PLAIN;
    char combined[DEBUG_BUFFER_LEN + 96];

    if (_filename_line_enabled) {
        snprintf(combined, sizeof(combined), "%s[%s:%ld] %s", prefix, loc.file_name(), loc.line(), msg);
    } else {
        snprintf(combined, sizeof(combined), "%s%s", prefix, msg);
    }

    _send(combined);
}
#else
void ElegantDebug::warning(const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

    const char* prefix = _color_enabled ? WARNING_TYPE : WARNING_TYPE_PLAIN;
    char combined[DEBUG_BUFFER_LEN + 32];

    snprintf(combined, sizeof(combined), "%s%s", prefix, msg);
    _send(combined);
}
#endif

void ElegantDebug::ok(const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

    const char* prefix = _color_enabled ? OK_TYPE : OK_TYPE_PLAIN;
    char combined[DEBUG_BUFFER_LEN + 32];
    snprintf(combined, sizeof(combined), "%s%s", prefix, msg);
    _send(combined);
}

void ElegantDebug::success(const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

    const char* prefix = _color_enabled ? SUCCESS_TYPE : SUCCESS_TYPE_PLAIN;
    char combined[DEBUG_BUFFER_LEN + 32];
    snprintf(combined, sizeof(combined), "%s%s", prefix, msg);
    _send(combined);
}

void ElegantDebug::info(const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

    const char* prefix = _color_enabled ? INFO_TYPE : INFO_TYPE_PLAIN;
    char combined[DEBUG_BUFFER_LEN + 32];
    snprintf(combined, sizeof(combined), "%s%s", prefix, msg);
    _send(combined);
}


// runtime helpers for 24‑bit ANSI colours; return a pointer to a static buffer
const char* ElegantDebug::customTextColor(uint8_t r, uint8_t g, uint8_t b) {
    static char ansi[24];
    snprintf(ansi, sizeof(ansi), "\033[38;2;%u;%u;%um", (unsigned)r, (unsigned)g, (unsigned)b);
    return ansi;
}

const char* ElegantDebug::customBgColor(uint8_t r, uint8_t g, uint8_t b) {
    static char ansi[24];
    snprintf(ansi, sizeof(ansi), "\033[48;2;%u;%u;%um", (unsigned)r, (unsigned)g, (unsigned)b);
    return ansi;
}



/* Deprecated functions, originally for macro-based logging *************************************

void ElegantDebug::_log(const char* file, int line, const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);
    char combined[DEBUG_BUFFER_LEN + 64];
    if (_filename_line_enabled) {
        snprintf(combined, sizeof(combined), "[%s:%d] %s", file, line, msg);
    } else {
        strncpy(combined, msg, sizeof(combined) - 1);
        combined[sizeof(combined) - 1] = '\0';
    }
    _send(combined);
}

void ElegantDebug::_logWithType(const char* file, int line, const char* type, const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);
    char combined[DEBUG_BUFFER_LEN + 96];
    if (_filename_line_enabled) {
        snprintf(combined, sizeof(combined), "[ %s ] [%s:%d] %s", file, line, type, msg);
    } else {
        snprintf(combined, sizeof(combined), "[ %s ] %s", type, msg);
    }
    _send(combined);
}

void ElegantDebug::_error(const char* file, int line, const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);
    const char* prefix = _color_enabled ? ERROR_TYPE : ERROR_TYPE_PLAIN;
    char combined[DEBUG_BUFFER_LEN + 96];
    if (_filename_line_enabled) {
        snprintf(combined, sizeof(combined), "%s[%s:%d] %s", prefix, file, line, msg);
    } else {
        snprintf(combined, sizeof(combined), "%s%s", prefix, msg);
    }
    _send(combined);
}

void ElegantDebug::_warning(const char* file, int line, const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);
    const char* prefix = _color_enabled ? WARNING_TYPE : WARNING_TYPE_PLAIN;
    char combined[DEBUG_BUFFER_LEN + 96];
    if (_filename_line_enabled) {
        snprintf(combined, sizeof(combined), "%s[%s:%d] %s", prefix, file, line, msg);
    } else {
        snprintf(combined, sizeof(combined), "%s%s", prefix, msg);
    }
    _send(combined);
}

void ElegantDebug::_ok(const char* file, int line, const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);
    const char* prefix = _color_enabled ? OK_TYPE : OK_TYPE_PLAIN;
    char combined[DEBUG_BUFFER_LEN + 96];
    if (_filename_line_enabled) {
        snprintf(combined, sizeof(combined), "%s[%s:%d] %s", prefix, file, line, msg);
    } else {
        snprintf(combined, sizeof(combined), "%s%s", prefix, msg);
    }
    _send(combined);
}

void ElegantDebug::_success(const char* file, int line, const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);
    const char* prefix = _color_enabled ? SUCCESS_TYPE : SUCCESS_TYPE_PLAIN;
    char combined[DEBUG_BUFFER_LEN + 96];
    if (_filename_line_enabled) {
        snprintf(combined, sizeof(combined), "%s[%s:%d] %s", prefix, file, line, msg);
    } else {
        snprintf(combined, sizeof(combined), "%s%s", prefix, msg);
    }
    _send(combined);
}

void ElegantDebug::_info(const char* file, int line, const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);
    const char* prefix = _color_enabled ? INFO_TYPE : INFO_TYPE_PLAIN;
    char combined[DEBUG_BUFFER_LEN + 96];
    if (_filename_line_enabled) {
        snprintf(combined, sizeof(combined), "%s[%s:%d] %s", prefix, file, line, msg);
    } else {
        snprintf(combined, sizeof(combined), "%s%s", prefix, msg);
    }
    _send(combined);
}
*********************************************************************************************/