// Copyright(c) 2019 ZVYAGIN.Alexander@gmail.com
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#include <spdlog/details/null_mutex.h>
#include <spdlog/details/os.h>
#include <spdlog/details/synchronous_factory.h>
#include <spdlog/sinks/base_sink.h>

#include <array>
#ifndef SD_JOURNAL_SUPPRESS_LOCATION
    #define SD_JOURNAL_SUPPRESS_LOCATION
#endif
#include <systemd/sd-journal.h>

namespace spdlog {
namespace sinks {

/**
 * Sink that write to systemd journal using the `sd_journal_send()` and `sd_journal_sendv()` library calls.
 */
template <typename Mutex>
class systemd_sink : public base_sink<Mutex> {
public:
    systemd_sink(std::string ident = "", bool enable_formatting = false)
        : ident_{std::move(ident)},
          enable_formatting_{enable_formatting},
          syslog_levels_{{/* spdlog::level::trace      */ LOG_DEBUG,
                          /* spdlog::level::debug      */ LOG_DEBUG,
                          /* spdlog::level::info       */ LOG_INFO,
                          /* spdlog::level::warn       */ LOG_WARNING,
                          /* spdlog::level::err        */ LOG_ERR,
                          /* spdlog::level::critical   */ LOG_CRIT,
                          /* spdlog::level::off        */ LOG_INFO}} {}

    ~systemd_sink() override {}

    systemd_sink(const systemd_sink &) = delete;
    systemd_sink &operator=(const systemd_sink &) = delete;

protected:
    const std::string ident_;
    bool enable_formatting_ = false;
    using levels_array = std::array<int, 7>;
    levels_array syslog_levels_;

    void sink_it_(const details::log_msg &msg) override {
        int err;
        string_view_t payload;
        memory_buf_t formatted;
        if (enable_formatting_) {
            base_sink<Mutex>::formatter_->format(msg, formatted);
            payload = string_view_t(formatted.data(), formatted.size());
        } else {
            payload = msg.payload;
        }

        const string_view_t syslog_identifier = ident_.empty() ? msg.logger_name : ident_;

#ifdef SPDLOG_NO_TLS
        size_t length = payload.size();
        // limit to max int
        if (length > static_cast<size_t>(std::numeric_limits<int>::max())) {
            length = static_cast<size_t>(std::numeric_limits<int>::max());
        }


        // Do not send source location if not available
        if (msg.source.empty()) {
            // Note: function call inside '()' to avoid macro expansion
            err = (sd_journal_send)("MESSAGE=%.*s", static_cast<int>(length), payload.data(),
                                    "PRIORITY=%d", syslog_level(msg.level),
#ifndef SPDLOG_NO_THREAD_ID
                                    "TID=%zu", msg.thread_id,
#endif
                                    "SYSLOG_IDENTIFIER=%.*s",
                                    static_cast<int>(syslog_identifier.size()),
                                    syslog_identifier.data(), nullptr);
        } else {
            err = (sd_journal_send)("MESSAGE=%.*s", static_cast<int>(length), payload.data(),
                                    "PRIORITY=%d", syslog_level(msg.level),
#ifndef SPDLOG_NO_THREAD_ID
                                    "TID=%zu", msg.thread_id,
#endif
                                    "SYSLOG_IDENTIFIER=%.*s",
                                    static_cast<int>(syslog_identifier.size()),
                                    syslog_identifier.data(), "CODE_FILE=%s", msg.source.filename,
                                    "CODE_LINE=%d", msg.source.line, "CODE_FUNC=%s",
                                    msg.source.funcname, nullptr);
        }

#else // SPDLOG_NO_TLS

        std::vector<iovec> iovector;
        const std::string message{std::string("MESSAGE=") + payload.data()};
        iovector.push_back({const_cast<char *>(message.c_str()), message.length()});

        const std::string priority{"PRIORITY=" + std::to_string(syslog_level(msg.level))};
        iovector.push_back({const_cast<char *>(priority.c_str()), priority.length()});
#ifndef SPDLOG_NO_THREAD_ID
        const std::string tid{"TID=" + std::to_string(msg.thread_id)};
        iovector.push_back({const_cast<char *>(tid.c_str()), tid.length()});
#endif
        const std::string syslog_id{std::string("SYSLOG_IDENTIFIER=") + syslog_identifier.data()};
        iovector.push_back({const_cast<char *>(syslog_id.c_str()), syslog_id.length()});

        std::string file, line, func;
        if (!msg.source.empty()) {
            file.assign(std::move(std::string("CODE_FILE=") + msg.source.filename));
            iovector.push_back({const_cast<char *>(file.c_str()), file.length()});
            line.assign(std::move(std::string("CODE_LINE=") + std::to_string(msg.source.line)));
            iovector.push_back({const_cast<char *>(line.c_str()), line.length()});
            func.assign(std::move(std::string("CODE_FUNC=") + msg.source.funcname));
            iovector.push_back({const_cast<char *>(func.c_str()), func.length()});
        }

        auto &context{spdlog::mdc::get_context()};
        std::vector<std::string> context_strings;
        context_strings.reserve(context.size());
        for (const auto &item : context) {
            std::string key;
            key.reserve(item.first.length() + item.second.length() + 1);
            key = item.first;
            std::transform(key.begin(), key.end(), key.begin(), ::toupper);
            key += '=' + item.second;
            context_strings.emplace_back(std::move(key));
        }
        for (const auto &item : context_strings) {
            iovector.push_back({const_cast<char *>(item.c_str()), item.length()});
        }

        err = sd_journal_sendv(iovector.data(), iovector.size());

#endif // SPDLOG_NO_TLS
        if (err) {
            throw_spdlog_ex("Failed writing to systemd", errno);
        }
    }

    int syslog_level(level::level_enum l) {
        return syslog_levels_.at(static_cast<levels_array::size_type>(l));
    }

    void flush_() override {}
};

using systemd_sink_mt = systemd_sink<std::mutex>;
using systemd_sink_st = systemd_sink<details::null_mutex>;
}  // namespace sinks

// Create and register a syslog logger
template <typename Factory = spdlog::synchronous_factory>
inline std::shared_ptr<logger> systemd_logger_mt(const std::string &logger_name,
                                                 const std::string &ident = "",
                                                 bool enable_formatting = false) {
    return Factory::template create<sinks::systemd_sink_mt>(logger_name, ident, enable_formatting);
}

template <typename Factory = spdlog::synchronous_factory>
inline std::shared_ptr<logger> systemd_logger_st(const std::string &logger_name,
                                                 const std::string &ident = "",
                                                 bool enable_formatting = false) {
    return Factory::template create<sinks::systemd_sink_st>(logger_name, ident, enable_formatting);
}
}  // namespace spdlog
