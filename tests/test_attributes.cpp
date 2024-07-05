#include "includes.h"
#include "spdlog/mdc.h"
#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/spdlog.h"
#include "test_sink.h"

TEST_CASE("Attribute test") {
    auto test_sink = std::make_shared<spdlog::sinks::test_sink_st>();
    spdlog::logger log_a("log_a", test_sink);
    spdlog::logger log_b("log_b", test_sink);
    log_a.set_pattern("[%n] [%*]");
    log_b.set_pattern("[%n] [%*]");

    log_a.attrs().put("my_key", "my_value");

    log_a.info("Hello");
    log_b.info("Hello");

    auto expected_log_a = spdlog::fmt_lib::format("[log_a] [my_key:my_value]");
    auto expected_log_b = spdlog::fmt_lib::format("[log_b] []");

    auto lines = test_sink->lines();
    REQUIRE(lines.size() == 2);
    REQUIRE(lines[0] == expected_log_a);
    REQUIRE(lines[1] == expected_log_b);
}