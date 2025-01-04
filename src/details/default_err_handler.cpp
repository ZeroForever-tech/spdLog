// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#include "iostream"
#include "spdlog/details/default_err_handler.h"
#include "spdlog/details/os.h"

namespace spdlog {
namespace details {

void default_err_handler::handle(const std::string &origin, const source_loc &loc, const std::string &err_msg) const {
    std::lock_guard lk{mutex_};
    const auto tm_time = os::localtime();
    char date_buf[128];
    std::strftime(date_buf, sizeof(date_buf), "%Y-%m-%d %H:%M:%S", &tm_time);
    std::string msg;
    if (loc.empty()) {
        msg = fmt_lib::format("[*** LOGGING ERROR ***] [{}] [{}] {}\n", date_buf, origin, err_msg);
    }
    else {
        msg = fmt_lib::format("[*** LOGGING ERROR ***] [{}({})] [{}] [{}] {}\n", loc.filename, loc.line, date_buf, origin, err_msg);
    }
    std::fputs(msg.c_str(), stderr);
}

}  // namespace details
}  // namespace spdlog
