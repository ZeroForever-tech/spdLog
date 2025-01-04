// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#include <string>
#include <mutex>
#include "spdlog/common.h"

// by default, prints the error to stderr. thread safe.
namespace spdlog {
namespace details {
class error_handler {
public:
    explicit error_handler(std::string name);
    error_handler(const error_handler &);
    error_handler(error_handler &&) noexcept;
    // for simplicity allow only construction of this class.
    // otherwise we need to deal with mutexes and potential deadlocks.
    error_handler &operator=(const error_handler &) = delete;
    error_handler &operator=(error_handler &&) = delete;
    void handle(const source_loc& loc, const std::string &err_msg) const;
    void set_name(const std::string& name);
    void set_custom_handler(err_handler handler);

private:
    mutable std::mutex mutex_;
    std::string name_;
    err_handler custom_handler_ = nullptr;
};


}} // namespace spdlog::details
