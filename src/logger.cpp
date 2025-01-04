// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#include "spdlog/logger.h"
#include <cstdio>

#include "spdlog/pattern_formatter.h"
#include "spdlog/sinks/sink.h"

namespace spdlog {

// public methods
logger::logger(const logger &other) noexcept
    : name_(other.name_),
      sinks_(other.sinks_),
      level_(other.level_.load(std::memory_order_relaxed)),
      flush_level_(other.flush_level_.load(std::memory_order_relaxed)),
      err_handler_(other.err_handler_) {}

logger::logger(logger &&other) noexcept
    : name_(std::move(other.name_)),
      sinks_(std::move(other.sinks_)),
      level_(other.level_.load(std::memory_order_relaxed)),
      flush_level_(other.flush_level_.load(std::memory_order_relaxed)),
      err_handler_(std::move(other.err_handler_)) {}

void logger::set_level(level level) { level_.store(level); }

level logger::log_level() const { return level_.load(std::memory_order_relaxed); }

const std::string &logger::name() const { return name_; }

// set formatting for the sinks in this logger.
// each sink will get a separate instance of the formatter object.
void logger::set_formatter(std::unique_ptr<formatter> f) {
    for (auto it = sinks_.begin(); it != sinks_.end(); ++it) {
        if (std::next(it) == sinks_.end()) {
            // last element - we can move it.
            (*it)->set_formatter(std::move(f));
            break;  // to prevent clang-tidy warning
        }
        (*it)->set_formatter(f->clone());
    }
}

void logger::set_pattern(std::string pattern, pattern_time_type time_type) {
    auto new_formatter = std::make_unique<pattern_formatter>(std::move(pattern), time_type);
    set_formatter(std::move(new_formatter));
}

// flush functions
void logger::flush() { flush_(); }

void logger::flush_on(level level) { flush_level_.store(level); }

level logger::flush_level() const { return flush_level_.load(std::memory_order_relaxed); }

// sinks
const std::vector<sink_ptr> &logger::sinks() const { return sinks_; }

std::vector<sink_ptr> &logger::sinks() { return sinks_; }

// custom error handler
void logger::set_error_handler(err_handler handler) {
    err_handler_.set_custom_handler(std::move(handler));
}

// create new logger with same sinks and configuration.
std::shared_ptr<logger> logger::clone(std::string logger_name) {
    auto cloned = std::make_shared<logger>(*this);
    cloned->name_ = std::move(logger_name);
    cloned->err_handler_.set_name(cloned->name_);
    return cloned;
}

// private/protected methods
void logger::flush_() {
    for (auto &sink : sinks_) {
        try {
            sink->flush();
        }
        catch (const std::exception &ex) {                                                                                    \
            err_handler_.handle(source_loc{}, ex.what());                                                                                           \
        }                                                                                                                     \
        catch (...) {                                                                                                         \
            err_handler_.handle(source_loc{}, "Unknown exception");                                                           \
        }
    }
}

bool logger::should_flush_(const details::log_msg &msg) const {
    auto flush_level = flush_level_.load(std::memory_order_relaxed);
    return (msg.log_level >= flush_level) && (msg.log_level != level::off);
}

}  // namespace spdlog
