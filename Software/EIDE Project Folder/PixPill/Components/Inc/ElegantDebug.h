/*******************************************************************************
 * @file    ElegantDebug.h
 * @version 1.3
 * @brief   ANSI-colored debug logging for STM32 (C++ header).
 *
 * This header declares the C++ `ElegantDebug` class used to send formatted log
 * messages over a HAL UART interface or USB-CDC interface. The class supports
 * optional timestamps and ANSI color output for terminals that support it.
 * A functionally equivalent C implementation is provided under `Src-C/`.
 *
 * Usage:
 *   - Include this header in your C++ source and construct `ElegantDebug` with a
 *     pointer to a configured `UART_HandleTypeDef`.
 *   - Call `log`, `info`, `error`, etc. to print messages.
 *
 * Notes:
 *   - Requires STM32Cube HAL UART module; enable `HAL_UART_MODULE_ENABLED`.
 *   - See repository README for examples and integration instructions.
 *
 * @author:    WilliTourt <willitourt@foxmail.com>
 * @date:      2026-03-01
 * 
 * @changelog:
 * - 2025-12-10: Initial release.
 * - 2025-12-11: Added support for filename and ln number in warning/error messages.
 *               But this feature is not available below C++20.
 *               Allowed custom styles for type prefix in logWithType().
 *               Added more ANSI escape codes for color/style.
 * - 2026-02-22: Added USB-CDC support. Now you can enable USB-CDC in CubeMX and use
 *               the same code to log messages over USB port. Need to set
 *               'USB_AS_DEBUG_PORT' macro to 1.
 * - 2026-03-01: Modified `COLOR_CUSTOM(r,g,b)` macro implementation to use
 *               `customTextColor(r,g,b)` public method. This allows to fill
 *               in the color values at runtime. Background colors too.
 * 
 ******************************************************************************/

#pragma once



#define USB_AS_DEBUG_PORT false // Set to 1 to use USB-CDC as debug port. 0 for UART



#include "main.h"

#include <cstdio>
#include <cstdarg>
#include <cstring>

#if __cplusplus >= 202002L
#include <source_location>
#endif

#if !defined(__STM32F0xx_HAL_H) && \
    !defined(__STM32F1xx_HAL_H) && \
    !defined(__STM32F2xx_HAL_H) && \
    !defined(__STM32F3xx_HAL_H) && \
    !defined(__STM32F4xx_HAL_H) && \
    !defined(__STM32F7xx_HAL_H) && \
    !defined(__STM32H5xx_HAL_H) && \
    !defined(__STM32H7xx_HAL_H) && \
    !defined(__STM32L0xx_HAL_H) && \
    !defined(__STM32L1xx_HAL_H) && \
    !defined(__STM32L4xx_HAL_H) && \
    !defined(__STM32G0xx_HAL_H) && \
    !defined(__STM32G4xx_HAL_H) && \
    !defined(__STM32WBxx_HAL_H) && \
    !defined(__STM32WLxx_HAL_H) && \
    !defined(__STM32MP1xx_HAL_H) && \
    !defined(STM32C0xx_HAL_H)
#error "This debugging library can only be used with STM32Cube HAL drivers"
#endif
// If you encounter this compilation error when including this library
// in a normal HAL project, it might be because I haven't fully listed
// all the macros of the STM32 series. If you encounter any, you are
// welcome to submit a PR or issue!

#if defined(HAL_UART_MODULE_ENABLED)
    #include "usart.h"
#else
    #error "At least one serial port should be opened"
#endif

#if (USB_AS_DEBUG_PORT == 1)
    #include "usbd_cdc_if.h"
#endif

/* ANSI escape codes for output *****************************************/

// Colors for texts
#define COLOR_DARK_RED      "\033[31m"
#define COLOR_DARK_GREEN    "\033[32m"
#define COLOR_DARK_YELLOW   "\033[33m"
#define COLOR_DARK_BLUE     "\033[34m"
#define COLOR_DARK_MAGENTA  "\033[35m"
#define COLOR_DARK_CYAN     "\033[36m"
#define COLOR_DARK_WHITE    "\033[37m"

#define COLOR_RED           "\033[91m"
#define COLOR_GREEN         "\033[92m"
#define COLOR_YELLOW        "\033[93m"
#define COLOR_BLUE          "\033[94m"
#define COLOR_MAGENTA       "\033[95m"
#define COLOR_CYAN          "\033[96m"
#define COLOR_WHITE         "\033[97m"

// #define COLOR_CUSTOM(r,g,b) "\033[38;2;" #r ";" #g ";" #b "m" // Custom 24-bit colors for text
#define COLOR_CUSTOM(r,g,b) ElegantDebug::customTextColor(r,g,b)

// Background colors
#define BG_RED              "\033[41m"
#define BG_GREEN            "\033[42m"
#define BG_YELLOW           "\033[43m"
#define BG_BLUE             "\033[44m"
#define BG_MAGENTA          "\033[45m"
#define BG_CYAN             "\033[46m"
#define BG_WHITE            "\033[47m"

// #define BG_COLOR_CUSTOM(r,g,b) "\033[48;2;" #r ";" #g ";" #b "m" // Custom 24-bit background colors
#define BG_COLOR_CUSTOM(r,g,b) ElegantDebug::customBgColor(r,g,b)

// Styles
#define BOLD                "\033[1m"
#define DIM                 "\033[2m"
#define ITALIC              "\033[3m"
#define UNDERLINE           "\033[4m"
#define BLINK               "\033[5m"
#define REVERSE             "\033[7m"
#define CONCEAL             "\033[8m"

// Clear specific color/styles
#define CLR                 "\033[0m"   // Clear color and style of text before this macro
#define CLR_TEXT_COLOR      "\033[39m"
#define CLR_BG_COLOR        "\033[49m"
#define CLR_BOLD            "\033[21m"
#define CLR_DIM             "\033[22m"
#define CLR_ITALIC          "\033[23m"
#define CLR_UNDERLINE       "\033[24m"
#define CLR_BLINK           "\033[25m"
#define CLR_REVERSE         "\033[27m"
#define CLR_CONCEAL         "\033[28m"

// Prefixes
#define ERROR_TYPE          "\033[91m\033[1m[ERROR]\033[0m "
#define WARNING_TYPE        "\033[93m\033[1m[WARNING]\033[0m "
#define INFO_TYPE           "\033[94m\033[1m[INFO]\033[0m "
#define OK_TYPE             "\033[92m\033[1m[OK]\033[0m "
#define SUCCESS_TYPE        "\033[92m\033[1m[SUCCESS]\033[0m "

#define ERROR_TYPE_PLAIN    "[ERROR] "
#define WARNING_TYPE_PLAIN  "[WARNING] "
#define INFO_TYPE_PLAIN     "[INFO] "
#define OK_TYPE_PLAIN       "[OK] "
#define SUCCESS_TYPE_PLAIN  "[SUCCESS] "

/************************************************************************/

#define DEBUG_BUFFER_LEN 256

class ElegantDebug {
    public:

        // Constructor: can enable/disable timestamp and color output globally
        #if __cplusplus < 202002L
            ElegantDebug(bool enable_timestamp = true, bool enable_color = true);
            ElegantDebug(UART_HandleTypeDef *huart,
                         bool enable_timestamp = true, bool enable_color = true);
        #else
            ElegantDebug(bool enable_timestamp = true, bool enable_color = true,
                         bool enable_filename_line = false);
            ElegantDebug(UART_HandleTypeDef *huart, bool enable_timestamp = true,
                         bool enable_color = true, bool enable_filename_line = false);
        #endif

        // Basic formatted log
        void log(const char* format, ...);

        // Log with a type prefix
        void logWithType(const char* type, const char* style, const char* format, ...);

        // Convenience helpers
        void ok(const char* format, ...);
        void success(const char* format, ...);
        void info(const char* format, ...);

    #if __cplusplus < 202002L
        // void logWithType(const char* type, const char* format, ...);
        void error(const char* format, ...);
        void warning(const char* format, ...);
    #else
        // void logWithType(const char* type, const char* format, std::source_location loc = std::source_location::current(), ...);
        void error(const char* format, std::source_location loc = std::source_location::current(), ...);
        void warning(const char* format, std::source_location loc = std::source_location::current(), ...);
    #endif

        static const char* customTextColor(uint8_t r, uint8_t g, uint8_t b);
        static const char* customBgColor(uint8_t r, uint8_t g, uint8_t b);

        inline void setTimestampEnabled(bool enabled) { _timestamp_enabled = enabled; }
        inline void setColorEnabled(bool enabled) { _color_enabled = enabled; }

        #if __cplusplus >= 202002L
        inline void setFilenameLineEnabled(bool enabled) { _filename_line_enabled = enabled; }
        #endif

    private:
        UART_HandleTypeDef *_huart;
        bool _timestamp_enabled;
        bool _color_enabled;

        #if __cplusplus >= 202002L
        bool _filename_line_enabled;
        #endif
        
        void _send(const char* text);

    // #if !__cpp_lib_source_location
    //     // If compiler doesn't support c++20 source_location, use macro to log with file and line number
    //     void _log(const char* file, int line, const char* format, ...);
    //     void _logWithType(const char* file, int line, const char* type, const char* format, ...);
    //     void _error(const char* file, int line, const char* format, ...);
    //     void _warning(const char* file, int line, const char* format, ...);
    //     void _ok(const char* file, int line, const char* format, ...);
    //     void _success(const char* file, int line, const char* format, ...);
    //     void _info(const char* file, int line, const char* format, ...);
    // #endif
};

// #if !__cpp_lib_source_location

// #define dbg_log(dbg_inst, ...)           (dbg_inst)._log(__FILE__, __LINE__, __VA_ARGS__)
// #define dbg_info(dbg_inst, ...)          (dbg_inst)._info(__FILE__, __LINE__, __VA_ARGS__)
// #define dbg_error(dbg_inst, ...)         (dbg_inst)._error(__FILE__, __LINE__, __VA_ARGS__)
// #define dbg_warning(dbg_inst, ...)       (dbg_inst)._warning(__FILE__, __LINE__, __VA_ARGS__)
// #define dbg_ok(dbg_inst, ...)            (dbg_inst)._ok(__FILE__, __LINE__, __VA_ARGS__)
// #define dbg_success(dbg_inst, ...)       (dbg_inst)._success(__FILE__, __LINE__, __VA_ARGS__)
// #define dbg_logtype(dbg_inst, type, ...) (dbg_inst)._logWithType(__FILE__, __LINE__, type, __VA_ARGS__)

// #endif
