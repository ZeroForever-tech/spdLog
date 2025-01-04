// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#include <string>
#include <mutex>
#include <exception>
#include "spdlog/common.h"

// by default, prints the error to stderr, thread safe
namespace spdlog {
namespace details {
class default_err_handler {
    mutable std::mutex mutex_;
public:
    void handle_ex(const std::string& origin, const source_loc& loc, const std::exception& ex) const;
};


}} // namespace spdlog::details
