//
// Copyright(c) 2015 Gabi Melman.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

// spdlog usage example

#include <chrono>
#include <cstdio>

void load_levels_example();
void stdout_logger_example();
void basic_example();
void rotating_example();
void daily_example();
void callback_example();
void async_example();
void binary_example();
void vector_example();
void stopwatch_example();
void trace_example();
void multi_sink_example();
void user_defined_example();
void err_handler_example();
void syslog_example();
void udp_example();
void custom_flags_example();
void file_events_example();
void replace_default_logger_example();

#include "spdlog/cfg/env.h"  // support for loading levels from the environment variable
#include "spdlog/spdlog.h"
#include "spdlog/version.h"

#include "fmt/xchar.h"
int main(int, char *[]) { 
    std::wstring wideStr = L"Hello, Wide World!";

    // Print directly to the console
    fmt::print(L"{}\n", wideStr);
}