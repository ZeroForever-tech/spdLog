// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#include "iostream"
#include "spdlog/details/err_helper.h"
#include "spdlog/details/os.h"

namespace spdlog {
namespace details {

// Prints error to stderr with source location (if available). A stderr sink is not used because reaching
// this point might indicate a problem with the logging system itself so we use fputs() directly.
void err_helper::handle_ex(const std::string &origin, const source_loc &loc, const std::exception &ex) const {
    if (custom_err_handler_) {
        custom_err_handler_(ex.what());
        return;
    }
    const auto tm_time = os::localtime();
    char date_buf[32];
    std::strftime(date_buf, sizeof(date_buf), "%Y-%m-%d %H:%M:%S", &tm_time);
    std::string msg;
    if (loc.empty()) {
        msg = fmt_lib::format("[*** LOG ERROR ***] [{}] [{}] {}\n", date_buf, origin, ex.what());
    } else {
        msg = fmt_lib::format("[*** LOG ERROR ***] [{}({})] [{}] [{}] {}\n", loc.filename, loc.line, date_buf, origin,
                              ex.what());
    }
    std::fputs(msg.c_str(), stderr);
}

void err_helper::handle_unknown_ex(const std::string &origin, const source_loc &loc) const {
    handle_ex(origin, loc, std::runtime_error("unknown exception"));
}

void err_helper::set_err_handler(err_handler handler) {
    custom_err_handler_ = std::move(handler);
}

}  // namespace details
}  // namespace spdlog
