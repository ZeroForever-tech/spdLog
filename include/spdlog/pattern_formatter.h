// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#include <spdlog/common.h>
#include <spdlog/details/fmt_helper.h>
#include <spdlog/details/log_msg.h>
#include <spdlog/details/os.h>
#ifndef SPDLOG_NO_TLS
    #include <spdlog/mdc.h>
#endif
#include <spdlog/formatter.h>

#include <chrono>
#include <ctime>
#include <memory>

#include <string>
#include <unordered_map>
#include <vector>

namespace spdlog {
namespace details {

// padding information.
struct padding_info {
    enum class pad_side { left, right, center };

    padding_info() = default;
    padding_info(size_t width, padding_info::pad_side side, bool truncate)
        : width_(width),
          side_(side),
          truncate_(truncate),
          enabled_(true) {}

    bool enabled() const { return enabled_; }
    size_t width_ = 0;
    pad_side side_ = pad_side::left;
    bool truncate_ = false;
    bool enabled_ = false;
};

class SPDLOG_API flag_formatter {
public:
    explicit flag_formatter(padding_info padinfo)
        : padinfo_(padinfo) {}
    flag_formatter() = default;
    virtual ~flag_formatter() = default;
    virtual void format(const details::log_msg &msg,
                        const std::tm &tm_time,
                        memory_buf_t &dest) = 0;

protected:
    padding_info padinfo_;
};

///////////////////////////////////////////////////////////////////////
// name & level pattern appender
///////////////////////////////////////////////////////////////////////

class scoped_padder {
public:
    scoped_padder(size_t wrapped_size, const padding_info &padinfo, memory_buf_t &dest)
        : padinfo_(padinfo),
          dest_(dest) {
        remaining_pad_ = static_cast<long>(padinfo.width_) - static_cast<long>(wrapped_size);
        if (remaining_pad_ <= 0) {
            return;
        }

        if (padinfo_.side_ == padding_info::pad_side::left) {
            pad_it(remaining_pad_);
            remaining_pad_ = 0;
        } else if (padinfo_.side_ == padding_info::pad_side::center) {
            auto half_pad = remaining_pad_ / 2;
            auto reminder = remaining_pad_ & 1;
            pad_it(half_pad);
            remaining_pad_ = half_pad + reminder;  // for the right side
        }
    }

    template <typename T>
    static unsigned int count_digits(T n) {
        return fmt_helper::count_digits(n);
    }

    ~scoped_padder() {
        if (remaining_pad_ >= 0) {
            pad_it(remaining_pad_);
        } else if (padinfo_.truncate_) {
            long new_size = static_cast<long>(dest_.size()) + remaining_pad_;
            if (new_size < 0) {
                new_size = 0;
            }
            dest_.resize(static_cast<size_t>(new_size));
        }
    }

private:
    void pad_it(long count) {
        fmt_helper::append_string_view(string_view_t(spaces_.data(), static_cast<size_t>(count)),
                                       dest_);
    }

    const padding_info &padinfo_;
    memory_buf_t &dest_;
    long remaining_pad_;
    string_view_t spaces_{"                                                                ", 64};
};

struct null_scoped_padder {
    null_scoped_padder(size_t /*wrapped_size*/,
                       const padding_info & /*padinfo*/,
                       memory_buf_t & /*dest*/) {}

    template <typename T>
    static unsigned int count_digits(T /* number */) {
        return 0;
    }
};

template <typename ScopedPadder>
class name_formatter final : public flag_formatter {
public:
    explicit name_formatter(padding_info padinfo)
        : flag_formatter(padinfo) {}

    void format(const details::log_msg &msg, const std::tm &, memory_buf_t &dest) override {
        ScopedPadder p(msg.logger_name.size(), padinfo_, dest);
        fmt_helper::append_string_view(msg.logger_name, dest);
    }
};

// log level appender
template <typename ScopedPadder>
class level_formatter final : public flag_formatter {
public:
    explicit level_formatter(padding_info padinfo)
        : flag_formatter(padinfo) {}

    void format(const details::log_msg &msg, const std::tm &, memory_buf_t &dest) override {
        const string_view_t &level_name = level::to_string_view(msg.level);
        ScopedPadder p(level_name.size(), padinfo_, dest);
        fmt_helper::append_string_view(level_name, dest);
    }
};

// short log level appender
template <typename ScopedPadder>
class short_level_formatter final : public flag_formatter {
public:
    explicit short_level_formatter(padding_info padinfo)
        : flag_formatter(padinfo) {}

    void format(const details::log_msg &msg, const std::tm &, memory_buf_t &dest) override {
        string_view_t level_name{level::to_short_c_str(msg.level)};
        ScopedPadder p(level_name.size(), padinfo_, dest);
        fmt_helper::append_string_view(level_name, dest);
    }
};

///////////////////////////////////////////////////////////////////////
// Date time pattern appenders
///////////////////////////////////////////////////////////////////////

static const char *ampm(const tm &t) { return t.tm_hour >= 12 ? "PM" : "AM"; }

static int to12h(const tm &t) { return t.tm_hour > 12 ? t.tm_hour - 12 : t.tm_hour; }

// Abbreviated weekday name
static std::array<const char *, 7> days{{"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"}};

template <typename ScopedPadder>
class a_formatter final : public flag_formatter {
public:
    explicit a_formatter(padding_info padinfo)
        : flag_formatter(padinfo) {}

    void format(const details::log_msg &, const std::tm &tm_time, memory_buf_t &dest) override {
        string_view_t field_value{days[static_cast<size_t>(tm_time.tm_wday)]};
        ScopedPadder p(field_value.size(), padinfo_, dest);
        fmt_helper::append_string_view(field_value, dest);
    }
};

// Full weekday name
static std::array<const char *, 7> full_days{
    {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"}};

template <typename ScopedPadder>
class A_formatter : public flag_formatter {
public:
    explicit A_formatter(padding_info padinfo)
        : flag_formatter(padinfo) {}

    void format(const details::log_msg &, const std::tm &tm_time, memory_buf_t &dest) override {
        string_view_t field_value{full_days[static_cast<size_t>(tm_time.tm_wday)]};
        ScopedPadder p(field_value.size(), padinfo_, dest);
        fmt_helper::append_string_view(field_value, dest);
    }
};

// Abbreviated month
static const std::array<const char *, 12> months{
    {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"}};

template <typename ScopedPadder>
class b_formatter final : public flag_formatter {
public:
    explicit b_formatter(padding_info padinfo)
        : flag_formatter(padinfo) {}

    void format(const details::log_msg &, const std::tm &tm_time, memory_buf_t &dest) override {
        string_view_t field_value{months[static_cast<size_t>(tm_time.tm_mon)]};
        ScopedPadder p(field_value.size(), padinfo_, dest);
        fmt_helper::append_string_view(field_value, dest);
    }
};

// Full month name
static const std::array<const char *, 12> full_months{{"January", "February", "March", "April",
                                                       "May", "June", "July", "August", "September",
                                                       "October", "November", "December"}};

template <typename ScopedPadder>
class B_formatter final : public flag_formatter {
public:
    explicit B_formatter(padding_info padinfo)
        : flag_formatter(padinfo) {}

    void format(const details::log_msg &, const std::tm &tm_time, memory_buf_t &dest) override {
        string_view_t field_value{full_months[static_cast<size_t>(tm_time.tm_mon)]};
        ScopedPadder p(field_value.size(), padinfo_, dest);
        fmt_helper::append_string_view(field_value, dest);
    }
};

// Date and time representation (Thu Aug 23 15:35:46 2014)
template <typename ScopedPadder>
class c_formatter final : public flag_formatter {
public:
    explicit c_formatter(padding_info padinfo)
        : flag_formatter(padinfo) {}

    void format(const details::log_msg &, const std::tm &tm_time, memory_buf_t &dest) override {
        const size_t field_size = 24;
        ScopedPadder p(field_size, padinfo_, dest);

        fmt_helper::append_string_view(days[static_cast<size_t>(tm_time.tm_wday)], dest);
        dest.push_back(' ');
        fmt_helper::append_string_view(months[static_cast<size_t>(tm_time.tm_mon)], dest);
        dest.push_back(' ');
        fmt_helper::append_int(tm_time.tm_mday, dest);
        dest.push_back(' ');
        // time

        fmt_helper::pad2(tm_time.tm_hour, dest);
        dest.push_back(':');
        fmt_helper::pad2(tm_time.tm_min, dest);
        dest.push_back(':');
        fmt_helper::pad2(tm_time.tm_sec, dest);
        dest.push_back(' ');
        fmt_helper::append_int(tm_time.tm_year + 1900, dest);
    }
};

// year - 2 digit
template <typename ScopedPadder>
class C_formatter final : public flag_formatter {
public:
    explicit C_formatter(padding_info padinfo)
        : flag_formatter(padinfo) {}

    void format(const details::log_msg &, const std::tm &tm_time, memory_buf_t &dest) override {
        const size_t field_size = 2;
        ScopedPadder p(field_size, padinfo_, dest);
        fmt_helper::pad2(tm_time.tm_year % 100, dest);
    }
};

// Short MM/DD/YY date, equivalent to %m/%d/%y 08/23/01
template <typename ScopedPadder>
class D_formatter final : public flag_formatter {
public:
    explicit D_formatter(padding_info padinfo)
        : flag_formatter(padinfo) {}

    void format(const details::log_msg &, const std::tm &tm_time, memory_buf_t &dest) override {
        const size_t field_size = 8;
        ScopedPadder p(field_size, padinfo_, dest);

        fmt_helper::pad2(tm_time.tm_mon + 1, dest);
        dest.push_back('/');
        fmt_helper::pad2(tm_time.tm_mday, dest);
        dest.push_back('/');
        fmt_helper::pad2(tm_time.tm_year % 100, dest);
    }
};

// year - 4 digit
template <typename ScopedPadder>
class Y_formatter final : public flag_formatter {
public:
    explicit Y_formatter(padding_info padinfo)
        : flag_formatter(padinfo) {}

    void format(const details::log_msg &, const std::tm &tm_time, memory_buf_t &dest) override {
        const size_t field_size = 4;
        ScopedPadder p(field_size, padinfo_, dest);
        fmt_helper::append_int(tm_time.tm_year + 1900, dest);
    }
};

// month 1-12
template <typename ScopedPadder>
class m_formatter final : public flag_formatter {
public:
    explicit m_formatter(padding_info padinfo)
        : flag_formatter(padinfo) {}

    void format(const details::log_msg &, const std::tm &tm_time, memory_buf_t &dest) override {
        const size_t field_size = 2;
        ScopedPadder p(field_size, padinfo_, dest);
        fmt_helper::pad2(tm_time.tm_mon + 1, dest);
    }
};

// day of month 1-31
template <typename ScopedPadder>
class d_formatter final : public flag_formatter {
public:
    explicit d_formatter(padding_info padinfo)
        : flag_formatter(padinfo) {}

    void format(const details::log_msg &, const std::tm &tm_time, memory_buf_t &dest) override {
        const size_t field_size = 2;
        ScopedPadder p(field_size, padinfo_, dest);
        fmt_helper::pad2(tm_time.tm_mday, dest);
    }
};

// hours in 24 format 0-23
template <typename ScopedPadder>
class H_formatter final : public flag_formatter {
public:
    explicit H_formatter(padding_info padinfo)
        : flag_formatter(padinfo) {}

    void format(const details::log_msg &, const std::tm &tm_time, memory_buf_t &dest) override {
        const size_t field_size = 2;
        ScopedPadder p(field_size, padinfo_, dest);
        fmt_helper::pad2(tm_time.tm_hour, dest);
    }
};

// hours in 12 format 1-12
template <typename ScopedPadder>
class I_formatter final : public flag_formatter {
public:
    explicit I_formatter(padding_info padinfo)
        : flag_formatter(padinfo) {}

    void format(const details::log_msg &, const std::tm &tm_time, memory_buf_t &dest) override {
        const size_t field_size = 2;
        ScopedPadder p(field_size, padinfo_, dest);
        fmt_helper::pad2(to12h(tm_time), dest);
    }
};

// minutes 0-59
template <typename ScopedPadder>
class M_formatter final : public flag_formatter {
public:
    explicit M_formatter(padding_info padinfo)
        : flag_formatter(padinfo) {}

    void format(const details::log_msg &, const std::tm &tm_time, memory_buf_t &dest) override {
        const size_t field_size = 2;
        ScopedPadder p(field_size, padinfo_, dest);
        fmt_helper::pad2(tm_time.tm_min, dest);
    }
};

// seconds 0-59
template <typename ScopedPadder>
class S_formatter final : public flag_formatter {
public:
    explicit S_formatter(padding_info padinfo)
        : flag_formatter(padinfo) {}

    void format(const details::log_msg &, const std::tm &tm_time, memory_buf_t &dest) override {
        const size_t field_size = 2;
        ScopedPadder p(field_size, padinfo_, dest);
        fmt_helper::pad2(tm_time.tm_sec, dest);
    }
};

// milliseconds
template <typename ScopedPadder>
class e_formatter final : public flag_formatter {
public:
    explicit e_formatter(padding_info padinfo)
        : flag_formatter(padinfo) {}

    void format(const details::log_msg &msg, const std::tm &, memory_buf_t &dest) override {
        auto millis = fmt_helper::time_fraction<std::chrono::milliseconds>(msg.time);
        const size_t field_size = 3;
        ScopedPadder p(field_size, padinfo_, dest);
        fmt_helper::pad3(static_cast<uint32_t>(millis.count()), dest);
    }
};

// microseconds
template <typename ScopedPadder>
class f_formatter final : public flag_formatter {
public:
    explicit f_formatter(padding_info padinfo)
        : flag_formatter(padinfo) {}

    void format(const details::log_msg &msg, const std::tm &, memory_buf_t &dest) override {
        auto micros = fmt_helper::time_fraction<std::chrono::microseconds>(msg.time);

        const size_t field_size = 6;
        ScopedPadder p(field_size, padinfo_, dest);
        fmt_helper::pad6(static_cast<size_t>(micros.count()), dest);
    }
};

// nanoseconds
template <typename ScopedPadder>
class F_formatter final : public flag_formatter {
public:
    explicit F_formatter(padding_info padinfo)
        : flag_formatter(padinfo) {}

    void format(const details::log_msg &msg, const std::tm &, memory_buf_t &dest) override {
        auto ns = fmt_helper::time_fraction<std::chrono::nanoseconds>(msg.time);
        const size_t field_size = 9;
        ScopedPadder p(field_size, padinfo_, dest);
        fmt_helper::pad9(static_cast<size_t>(ns.count()), dest);
    }
};

// seconds since epoch
template <typename ScopedPadder>
class E_formatter final : public flag_formatter {
public:
    explicit E_formatter(padding_info padinfo)
        : flag_formatter(padinfo) {}

    void format(const details::log_msg &msg, const std::tm &, memory_buf_t &dest) override {
        const size_t field_size = 10;
        ScopedPadder p(field_size, padinfo_, dest);
        auto duration = msg.time.time_since_epoch();
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
        fmt_helper::append_int(seconds, dest);
    }
};

// AM/PM
template <typename ScopedPadder>
class p_formatter final : public flag_formatter {
public:
    explicit p_formatter(padding_info padinfo)
        : flag_formatter(padinfo) {}

    void format(const details::log_msg &, const std::tm &tm_time, memory_buf_t &dest) override {
        const size_t field_size = 2;
        ScopedPadder p(field_size, padinfo_, dest);
        fmt_helper::append_string_view(ampm(tm_time), dest);
    }
};

// 12 hour clock 02:55:02 pm
template <typename ScopedPadder>
class r_formatter final : public flag_formatter {
public:
    explicit r_formatter(padding_info padinfo)
        : flag_formatter(padinfo) {}

    void format(const details::log_msg &, const std::tm &tm_time, memory_buf_t &dest) override {
        const size_t field_size = 11;
        ScopedPadder p(field_size, padinfo_, dest);

        fmt_helper::pad2(to12h(tm_time), dest);
        dest.push_back(':');
        fmt_helper::pad2(tm_time.tm_min, dest);
        dest.push_back(':');
        fmt_helper::pad2(tm_time.tm_sec, dest);
        dest.push_back(' ');
        fmt_helper::append_string_view(ampm(tm_time), dest);
    }
};

// 24-hour HH:MM time, equivalent to %H:%M
template <typename ScopedPadder>
class R_formatter final : public flag_formatter {
public:
    explicit R_formatter(padding_info padinfo)
        : flag_formatter(padinfo) {}

    void format(const details::log_msg &, const std::tm &tm_time, memory_buf_t &dest) override {
        const size_t field_size = 5;
        ScopedPadder p(field_size, padinfo_, dest);

        fmt_helper::pad2(tm_time.tm_hour, dest);
        dest.push_back(':');
        fmt_helper::pad2(tm_time.tm_min, dest);
    }
};

// ISO 8601 time format (HH:MM:SS), equivalent to %H:%M:%S
template <typename ScopedPadder>
class T_formatter final : public flag_formatter {
public:
    explicit T_formatter(padding_info padinfo)
        : flag_formatter(padinfo) {}

    void format(const details::log_msg &, const std::tm &tm_time, memory_buf_t &dest) override {
        const size_t field_size = 8;
        ScopedPadder p(field_size, padinfo_, dest);

        fmt_helper::pad2(tm_time.tm_hour, dest);
        dest.push_back(':');
        fmt_helper::pad2(tm_time.tm_min, dest);
        dest.push_back(':');
        fmt_helper::pad2(tm_time.tm_sec, dest);
    }
};

// ISO 8601 offset from UTC in timezone (+-HH:MM)
template <typename ScopedPadder>
class z_formatter final : public flag_formatter {
public:
    explicit z_formatter(padding_info padinfo)
        : flag_formatter(padinfo) {}

    z_formatter() = default;
    z_formatter(const z_formatter &) = delete;
    z_formatter &operator=(const z_formatter &) = delete;

    void format(const details::log_msg &msg, const std::tm &tm_time, memory_buf_t &dest) override {
        const size_t field_size = 6;
        ScopedPadder p(field_size, padinfo_, dest);

        auto total_minutes = get_cached_offset(msg, tm_time);
        bool is_negative = total_minutes < 0;
        if (is_negative) {
            total_minutes = -total_minutes;
            dest.push_back('-');
        } else {
            dest.push_back('+');
        }

        fmt_helper::pad2(total_minutes / 60, dest);  // hours
        dest.push_back(':');
        fmt_helper::pad2(total_minutes % 60, dest);  // minutes
    }

private:
    log_clock::time_point last_update_{std::chrono::seconds(0)};
    int offset_minutes_{0};

    int get_cached_offset(const log_msg &msg, const std::tm &tm_time) {
        // refresh every 10 seconds
        if (msg.time - last_update_ >= std::chrono::seconds(10)) {
            offset_minutes_ = os::utc_minutes_offset(tm_time);
            last_update_ = msg.time;
        }
        return offset_minutes_;
    }
};

// Thread id
template <typename ScopedPadder>
class t_formatter final : public flag_formatter {
public:
    explicit t_formatter(padding_info padinfo)
        : flag_formatter(padinfo) {}

    void format(const details::log_msg &msg, const std::tm &, memory_buf_t &dest) override {
        const auto field_size = ScopedPadder::count_digits(msg.thread_id);
        ScopedPadder p(field_size, padinfo_, dest);
        fmt_helper::append_int(msg.thread_id, dest);
    }
};

// Current pid
template <typename ScopedPadder>
class pid_formatter final : public flag_formatter {
public:
    explicit pid_formatter(padding_info padinfo)
        : flag_formatter(padinfo) {}

    void format(const details::log_msg &, const std::tm &, memory_buf_t &dest) override {
        const auto pid = static_cast<uint32_t>(details::os::pid());
        auto field_size = ScopedPadder::count_digits(pid);
        ScopedPadder p(field_size, padinfo_, dest);
        fmt_helper::append_int(pid, dest);
    }
};

template <typename ScopedPadder>
class v_formatter final : public flag_formatter {
public:
    explicit v_formatter(padding_info padinfo)
        : flag_formatter(padinfo) {}

    void format(const details::log_msg &msg, const std::tm &, memory_buf_t &dest) override {
        ScopedPadder p(msg.payload.size(), padinfo_, dest);
        fmt_helper::append_string_view(msg.payload, dest);
    }
};

class ch_formatter final : public flag_formatter {
public:
    explicit ch_formatter(char ch)
        : ch_(ch) {}

    void format(const details::log_msg &, const std::tm &, memory_buf_t &dest) override {
        dest.push_back(ch_);
    }

private:
    char ch_;
};

// aggregate user chars to display as is
class aggregate_formatter final : public flag_formatter {
public:
    aggregate_formatter() = default;

    void add_ch(char ch) { str_ += ch; }
    void format(const details::log_msg &, const std::tm &, memory_buf_t &dest) override {
        fmt_helper::append_string_view(str_, dest);
    }

private:
    std::string str_;
};

// mark the color range. expect it to be in the form of "%^colored text%$"
class color_start_formatter final : public flag_formatter {
public:
    explicit color_start_formatter(padding_info padinfo)
        : flag_formatter(padinfo) {}

    void format(const details::log_msg &msg, const std::tm &, memory_buf_t &dest) override {
        msg.color_range_start = dest.size();
    }
};

class color_stop_formatter final : public flag_formatter {
public:
    explicit color_stop_formatter(padding_info padinfo)
        : flag_formatter(padinfo) {}

    void format(const details::log_msg &msg, const std::tm &, memory_buf_t &dest) override {
        msg.color_range_end = dest.size();
    }
};

// print source location
template <typename ScopedPadder>
class source_location_formatter final : public flag_formatter {
public:
    explicit source_location_formatter(padding_info padinfo)
        : flag_formatter(padinfo) {}

    void format(const details::log_msg &msg, const std::tm &, memory_buf_t &dest) override {
        if (msg.source.empty()) {
            ScopedPadder p(0, padinfo_, dest);
            return;
        }

        size_t text_size;
        if (padinfo_.enabled()) {
            // calc text size for padding based on "filename:line"
            text_size = std::char_traits<char>::length(msg.source.filename) +
                        ScopedPadder::count_digits(msg.source.line) + 1;
        } else {
            text_size = 0;
        }

        ScopedPadder p(text_size, padinfo_, dest);
        fmt_helper::append_string_view(msg.source.filename, dest);
        dest.push_back(':');
        fmt_helper::append_int(msg.source.line, dest);
    }
};

// print source filename
template <typename ScopedPadder>
class source_filename_formatter final : public flag_formatter {
public:
    explicit source_filename_formatter(padding_info padinfo)
        : flag_formatter(padinfo) {}

    void format(const details::log_msg &msg, const std::tm &, memory_buf_t &dest) override {
        if (msg.source.empty()) {
            ScopedPadder p(0, padinfo_, dest);
            return;
        }
        size_t text_size =
            padinfo_.enabled() ? std::char_traits<char>::length(msg.source.filename) : 0;
        ScopedPadder p(text_size, padinfo_, dest);
        fmt_helper::append_string_view(msg.source.filename, dest);
    }
};

template <typename ScopedPadder>
class short_filename_formatter final : public flag_formatter {
public:
    explicit short_filename_formatter(padding_info padinfo)
        : flag_formatter(padinfo) {}

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4127)  // consider using 'if constexpr' instead
#endif                               // _MSC_VER
    static const char *basename(const char *filename) {
        // if the size is 2 (1 character + null terminator) we can use the more efficient strrchr
        // the branch will be elided by optimizations
        if (sizeof(os::folder_seps) == 2) {
            const char *rv = std::strrchr(filename, os::folder_seps[0]);
            return rv != nullptr ? rv + 1 : filename;
        } else {
            const std::reverse_iterator<const char *> begin(filename + std::strlen(filename));
            const std::reverse_iterator<const char *> end(filename);

            const auto it = std::find_first_of(begin, end, std::begin(os::folder_seps),
                                               std::end(os::folder_seps) - 1);
            return it != end ? it.base() : filename;
        }
    }
#ifdef _MSC_VER
    #pragma warning(pop)
#endif  // _MSC_VER

    void format(const details::log_msg &msg, const std::tm &, memory_buf_t &dest) override {
        if (msg.source.empty()) {
            ScopedPadder p(0, padinfo_, dest);
            return;
        }
        auto filename = basename(msg.source.filename);
        size_t text_size = padinfo_.enabled() ? std::char_traits<char>::length(filename) : 0;
        ScopedPadder p(text_size, padinfo_, dest);
        fmt_helper::append_string_view(filename, dest);
    }
};

template <typename ScopedPadder>
class source_linenum_formatter final : public flag_formatter {
public:
    explicit source_linenum_formatter(padding_info padinfo)
        : flag_formatter(padinfo) {}

    void format(const details::log_msg &msg, const std::tm &, memory_buf_t &dest) override {
        if (msg.source.empty()) {
            ScopedPadder p(0, padinfo_, dest);
            return;
        }

        auto field_size = ScopedPadder::count_digits(msg.source.line);
        ScopedPadder p(field_size, padinfo_, dest);
        fmt_helper::append_int(msg.source.line, dest);
    }
};

// print source funcname
template <typename ScopedPadder>
class source_funcname_formatter final : public flag_formatter {
public:
    explicit source_funcname_formatter(padding_info padinfo)
        : flag_formatter(padinfo) {}

    void format(const details::log_msg &msg, const std::tm &, memory_buf_t &dest) override {
        if (msg.source.empty()) {
            ScopedPadder p(0, padinfo_, dest);
            return;
        }
        size_t text_size =
            padinfo_.enabled() ? std::char_traits<char>::length(msg.source.funcname) : 0;
        ScopedPadder p(text_size, padinfo_, dest);
        fmt_helper::append_string_view(msg.source.funcname, dest);
    }
};

// print elapsed time since last message
template <typename ScopedPadder, typename Units>
class elapsed_formatter final : public flag_formatter {
public:
    using DurationUnits = Units;

    explicit elapsed_formatter(padding_info padinfo)
        : flag_formatter(padinfo),
          last_message_time_(log_clock::now()) {}

    void format(const details::log_msg &msg, const std::tm &, memory_buf_t &dest) override {
        auto delta = (std::max)(msg.time - last_message_time_, log_clock::duration::zero());
        auto delta_units = std::chrono::duration_cast<DurationUnits>(delta);
        last_message_time_ = msg.time;
        auto delta_count = static_cast<size_t>(delta_units.count());
        auto n_digits = static_cast<size_t>(ScopedPadder::count_digits(delta_count));
        ScopedPadder p(n_digits, padinfo_, dest);
        fmt_helper::append_int(delta_count, dest);
    }

private:
    log_clock::time_point last_message_time_;
};

// Class for formatting Mapped Diagnostic Context (MDC) in log messages.
// Example: [logger-name] [info] [mdc_key_1:mdc_value_1 mdc_key_2:mdc_value_2] some message
#ifndef SPDLOG_NO_TLS
template <typename ScopedPadder>
class mdc_formatter : public flag_formatter {
public:
    explicit mdc_formatter(padding_info padinfo)
        : flag_formatter(padinfo) {}

    void format(const details::log_msg &, const std::tm &, memory_buf_t &dest) override {
        auto &mdc_map = mdc::get_context();
        if (mdc_map.empty()) {
            ScopedPadder p(0, padinfo_, dest);
            return;
        } else {
            format_mdc(mdc_map, dest);
        }
    }

    void format_mdc(const mdc::mdc_map_t &mdc_map, memory_buf_t &dest) {
        auto last_element = --mdc_map.end();
        for (auto it = mdc_map.begin(); it != mdc_map.end(); ++it) {
            auto &pair = *it;
            const auto &key = pair.first;
            const auto &value = pair.second;
            size_t content_size = key.size() + value.size() + 1;  // 1 for ':'

            if (it != last_element) {
                content_size++;  // 1 for ' '
            }

            ScopedPadder p(content_size, padinfo_, dest);
            fmt_helper::append_string_view(key, dest);
            fmt_helper::append_string_view(":", dest);
            fmt_helper::append_string_view(value, dest);
            if (it != last_element) {
                fmt_helper::append_string_view(" ", dest);
            }
        }
    }
};
#endif

// Full info formatter
// pattern: [%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [%s:%#] %v
class full_formatter final : public flag_formatter {
public:
    explicit full_formatter(padding_info padinfo)
        : flag_formatter(padinfo) {}

    void format(const details::log_msg &msg, const std::tm &tm_time, memory_buf_t &dest) override {
        using std::chrono::duration_cast;
        using std::chrono::milliseconds;
        using std::chrono::seconds;

        // cache the date/time part for the next second.
        auto duration = msg.time.time_since_epoch();
        auto secs = duration_cast<seconds>(duration);

        if (cache_timestamp_ != secs || cached_datetime_.size() == 0) {
            cached_datetime_.clear();
            cached_datetime_.push_back('[');
            fmt_helper::append_int(tm_time.tm_year + 1900, cached_datetime_);
            cached_datetime_.push_back('-');

            fmt_helper::pad2(tm_time.tm_mon + 1, cached_datetime_);
            cached_datetime_.push_back('-');

            fmt_helper::pad2(tm_time.tm_mday, cached_datetime_);
            cached_datetime_.push_back(' ');

            fmt_helper::pad2(tm_time.tm_hour, cached_datetime_);
            cached_datetime_.push_back(':');

            fmt_helper::pad2(tm_time.tm_min, cached_datetime_);
            cached_datetime_.push_back(':');

            fmt_helper::pad2(tm_time.tm_sec, cached_datetime_);
            cached_datetime_.push_back('.');

            cache_timestamp_ = secs;
        }
        dest.append(cached_datetime_.begin(), cached_datetime_.end());

        auto millis = fmt_helper::time_fraction<milliseconds>(msg.time);
        fmt_helper::pad3(static_cast<uint32_t>(millis.count()), dest);
        dest.push_back(']');
        dest.push_back(' ');

        // append logger name if exists
        if (msg.logger_name.size() > 0) {
            dest.push_back('[');
            fmt_helper::append_string_view(msg.logger_name, dest);
            dest.push_back(']');
            dest.push_back(' ');
        }

        dest.push_back('[');
        // wrap the level name with color
        msg.color_range_start = dest.size();
        // fmt_helper::append_string_view(level::to_c_str(msg.level), dest);
        fmt_helper::append_string_view(level::to_string_view(msg.level), dest);
        msg.color_range_end = dest.size();
        dest.push_back(']');
        dest.push_back(' ');

        // add source location if present
        if (!msg.source.empty()) {
            dest.push_back('[');
            const char *filename =
                details::short_filename_formatter<details::null_scoped_padder>::basename(
                    msg.source.filename);
            fmt_helper::append_string_view(filename, dest);
            dest.push_back(':');
            fmt_helper::append_int(msg.source.line, dest);
            dest.push_back(']');
            dest.push_back(' ');
        }

#ifndef SPDLOG_NO_TLS
        // add mdc if present
        auto &mdc_map = mdc::get_context();
        if (!mdc_map.empty()) {
            dest.push_back('[');
            mdc_formatter_.format_mdc(mdc_map, dest);
            dest.push_back(']');
            dest.push_back(' ');
        }
#endif
        // fmt_helper::append_string_view(msg.msg(), dest);
        fmt_helper::append_string_view(msg.payload, dest);
    }

private:
    std::chrono::seconds cache_timestamp_{0};
    memory_buf_t cached_datetime_;

#ifndef SPDLOG_NO_TLS
    mdc_formatter<null_scoped_padder> mdc_formatter_{padding_info{}};
#endif
};

}  // namespace details

class SPDLOG_API custom_flag_formatter : public details::flag_formatter {
public:
    virtual std::unique_ptr<custom_flag_formatter> clone() const = 0;

    void set_padding_info(const details::padding_info &padding) {
        flag_formatter::padinfo_ = padding;
    }
};

class SPDLOG_API pattern_formatter final : public formatter {
public:
    using custom_flags = std::unordered_map<char, std::unique_ptr<custom_flag_formatter>>;

    explicit pattern_formatter(std::string pattern,
                               pattern_time_type time_type = pattern_time_type::local,
                               std::string eol = spdlog::details::os::default_eol,
                               custom_flags custom_user_flags = custom_flags());

    // use default pattern is not given
    explicit pattern_formatter(pattern_time_type time_type = pattern_time_type::local,
                               std::string eol = spdlog::details::os::default_eol);

    pattern_formatter(const pattern_formatter &other) = delete;
    pattern_formatter &operator=(const pattern_formatter &other) = delete;

    std::unique_ptr<formatter> clone() const override;
    void format(const details::log_msg &msg, memory_buf_t &dest) override;

    template <typename T, typename... Args>
    pattern_formatter &add_flag(char flag, Args &&...args) {
        custom_handlers_[flag] = details::make_unique<T>(std::forward<Args>(args)...);
        return *this;
    }
    void set_pattern(std::string pattern);
    void need_localtime(bool need = true);

private:
    std::string pattern_;
    std::string eol_;
    pattern_time_type pattern_time_type_;
    bool need_localtime_;
    std::tm cached_tm_;
    std::chrono::seconds last_log_secs_;
    std::vector<std::unique_ptr<details::flag_formatter>> formatters_;
    custom_flags custom_handlers_;

    std::tm get_time_(const details::log_msg &msg);
    template <typename Padder>
    void handle_flag_(char flag, details::padding_info padding);

    // Extract given pad spec (e.g. %8X)
    // Advance the given it pass the end of the padding spec found (if any)
    // Return padding.
    static details::padding_info handle_padspec_(std::string::const_iterator &it,
                                                 std::string::const_iterator end);

    void compile_pattern_(const std::string &pattern);
};
}  // namespace spdlog

#ifdef SPDLOG_HEADER_ONLY
    #include "pattern_formatter-inl.h"
#endif
