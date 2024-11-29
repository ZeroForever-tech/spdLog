// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <exception>
#include <functional>
#include <initializer_list>
#include <memory>
#include <string>
#include <type_traits>

#include "./source_loc.h"
#include "./spdlog_config.h"

#if __has_include(<version>)
    #include <version>
#endif

#if defined(SPDLOG_SHARED_LIB)
    #if defined(_WIN32)
        #ifdef spdlog_EXPORTS
            #define SPDLOG_API __declspec(dllexport)
        #else  // !spdlog_EXPORTS
            #define SPDLOG_API __declspec(dllimport)
        #endif
    #else  // !defined(_WIN32)
        #define SPDLOG_API __attribute__((visibility("default")))
    #endif
#else  // !defined(SPDLOG_SHARED_LIB)
    #define SPDLOG_API
#endif

#include "fmt/fmt.h"

#define SPDLOG_FMT_RUNTIME(format_string) fmt::runtime(format_string)
#define SPDLOG_FMT_STRING(format_string) FMT_STRING(format_string)
#if defined(SPDLOG_WCHAR_FILENAMES)
    #include "fmt/xchar.h"
#endif

#ifndef SPDLOG_FUNCTION
    #define SPDLOG_FUNCTION static_cast<const char *>(__FUNCTION__)
#endif

#ifdef SPDLOG_NO_EXCEPTIONS
    #define SPDLOG_TRY
    #define SPDLOG_THROW(ex)                               \
        do {                                               \
            printf("spdlog fatal error: %s\n", ex.what()); \
            std::abort();                                  \
        } while (0)
    #define SPDLOG_CATCH_STD
#else
    #define SPDLOG_TRY try
    #define SPDLOG_THROW(ex) throw(ex)
    #define SPDLOG_CATCH_STD             \
        catch (const std::exception &) { \
        }
#endif

namespace spdlog {

class formatter;

namespace sinks {
class sink;
}

#if defined(_WIN32) && defined(SPDLOG_WCHAR_FILENAMES)
using filename_t = std::wstring;
// allow macro expansion to occur in SPDLOG_FILENAME_T
    #define SPDLOG_FILENAME_T_INNER(s) L##s
    #define SPDLOG_FILENAME_T(s) SPDLOG_FILENAME_T_INNER(s)
#else
using filename_t = std::string;
    #define SPDLOG_FILENAME_T(s) s
#endif

using log_clock = std::chrono::system_clock;
using sink_ptr = std::shared_ptr<sinks::sink>;
using sinks_init_list = std::initializer_list<sink_ptr>;
using err_handler = std::function<void(const std::string &err_msg)>;

using string_view_t = std::basic_string_view<char>;
using wstring_view_t = std::basic_string_view<wchar_t>;

namespace fmt_lib = fmt;
using memory_buf_t = fmt::basic_memory_buffer<char, 250>;
using wmemory_buf_t = fmt::basic_memory_buffer<wchar_t, 250>;

template <typename... Args>
using format_string_t = fmt::format_string<Args...>;

#define SPDLOG_BUF_TO_STRING(x) fmt::to_string(x)
#define SPDLOG_LEVEL_TRACE 0
#define SPDLOG_LEVEL_DEBUG 1
#define SPDLOG_LEVEL_INFO 2
#define SPDLOG_LEVEL_WARN 3
#define SPDLOG_LEVEL_ERROR 4
#define SPDLOG_LEVEL_CRITICAL 5
#define SPDLOG_LEVEL_OFF 6

#if !defined(SPDLOG_ACTIVE_LEVEL)
    #define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
#endif

// Log level enum
enum class level {
    trace = SPDLOG_LEVEL_TRACE,
    debug = SPDLOG_LEVEL_DEBUG,
    info = SPDLOG_LEVEL_INFO,
    warn = SPDLOG_LEVEL_WARN,
    err = SPDLOG_LEVEL_ERROR,
    critical = SPDLOG_LEVEL_CRITICAL,
    off = SPDLOG_LEVEL_OFF,
    n_levels
};

#if defined(SPDLOG_NO_ATOMIC_LEVELS)
using atomic_level_t = details::null_atomic<level>;
#else
using atomic_level_t = std::atomic<level>;
#endif

[[nodiscard]] constexpr size_t level_to_number(level lvl) noexcept { return static_cast<size_t>(lvl); }

constexpr auto levels_count = level_to_number(level::n_levels);
constexpr std::array<std::string_view, levels_count> level_string_views{"trace", "debug",    "info", "warning",
                                                                        "error", "critical", "off"};
constexpr std::array<std::string_view, levels_count> short_level_names{"T", "D", "I", "W", "E", "C", "O"};

[[nodiscard]] constexpr std::string_view to_string_view(spdlog::level lvl) noexcept {
    return level_string_views.at(level_to_number(lvl));
}

[[nodiscard]] constexpr const std::string_view to_short_string_view(spdlog::level lvl) noexcept {
    return short_level_names.at(level_to_number(lvl));
}

[[nodiscard]] SPDLOG_API spdlog::level level_from_str(const std::string &name) noexcept;

//
// Color mode used by sinks with color support.
//
enum class color_mode { always, automatic, never };

//
// Pattern time - specific time getting to use for pattern_formatter.
// local time by default
//
enum class pattern_time_type {
    local,  // log localtime
    utc     // log utc
};

//
// Log exception
//
class SPDLOG_API spdlog_ex : public std::exception {
public:
    explicit spdlog_ex(std::string msg);

    spdlog_ex(const std::string &msg, int last_errno);

    [[nodiscard]] const char *what() const noexcept override;

private:
    std::string msg_;
};

[[noreturn]] SPDLOG_API void throw_spdlog_ex(const std::string &msg, int last_errno);
[[noreturn]] SPDLOG_API void throw_spdlog_ex(std::string msg);

struct file_event_handlers {
    file_event_handlers()
        : before_open(nullptr),
          after_open(nullptr),
          before_close(nullptr),
          after_close(nullptr) {}

    std::function<void(const filename_t &filename)> before_open;
    std::function<void(const filename_t &filename, std::FILE *file_stream)> after_open;
    std::function<void(const filename_t &filename, std::FILE *file_stream)> before_close;
    std::function<void(const filename_t &filename)> after_close;
};

namespace details {

// to_string_view

[[nodiscard]] constexpr spdlog::string_view_t to_string_view(const memory_buf_t &buf) noexcept {
    return spdlog::string_view_t{buf.data(), buf.size()};
}

[[nodiscard]] constexpr spdlog::string_view_t to_string_view(spdlog::string_view_t str) noexcept { return str; }

#if defined(SPDLOG_WCHAR_FILENAMES)
[[nodiscard]] constexpr spdlog::wstring_view_t to_string_view(const wmemory_buf_t &buf) noexcept {
    return spdlog::wstring_view_t{buf.data(), buf.size()};
}

[[nodiscard]] constexpr spdlog::wstring_view_t to_string_view(spdlog::wstring_view_t str) noexcept { return str; }
#endif

template <typename T, typename... Args>
[[nodiscard]] constexpr fmt::basic_string_view<T> to_string_view(fmt::basic_format_string<T, Args...> fmt) noexcept {
    return fmt;
}

}  // namespace details
}  // namespace spdlog
