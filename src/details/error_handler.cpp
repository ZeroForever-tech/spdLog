// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#include "spdlog/details/error_handler.h"
#include "spdlog/details/os.h"
#include "iostream"

namespace spdlog {
namespace details {

error_handler::error_handler(std::string name)
    : name_(std::move(name)) {}

error_handler::error_handler(const error_handler &other) {
    std::lock_guard lk{other.mutex_};
    name_ = other.name_;
    custom_handler_ = other.custom_handler_;
}

error_handler::error_handler(error_handler &&other) noexcept {
    std::lock_guard lk{other.mutex_};
    name_ = std::move(other.name_);
    custom_handler_ = std::move(other.custom_handler_);
}

void error_handler::handle(const source_loc &loc, const std::string &err_msg) const {
    std::lock_guard lk{mutex_};
    if (custom_handler_) {
        custom_handler_(err_msg);
        return;
    }
    const auto tm_time = os::localtime();
    char date_buf[128];
    std::strftime(date_buf, sizeof(date_buf), "%Y-%m-%d %H:%M:%S", &tm_time);
    std::string msg;
    if (loc.empty()) {
        msg = fmt_lib::format("[*** LOGGING ERROR ***] [{}] [{}] {}\n", date_buf, name_, err_msg);
    }
    else {
        msg = fmt_lib::format("[*** LOGGING ERROR ***] [{}({})] [{}] [{}] {}\n", loc.filename, loc.line, date_buf, name_, err_msg);
    }
    std::fputs(msg.c_str(), stderr);
}
void error_handler::set_name(const std::string &name) {
    std::lock_guard lk{mutex_};
    name_ = name;
}

void error_handler::set_custom_handler(err_handler handler) {
    std::lock_guard lk{mutex_};
    custom_handler_ = std::move(handler);
}

}  // namespace details
}  // namespace spdlog
