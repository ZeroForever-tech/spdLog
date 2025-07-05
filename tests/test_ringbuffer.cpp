#include "includes.h"
#include "spdlog/sinks/ringbuffer_sink.h"

TEST_CASE("ringbuffer invalid size", "[ringbuffer]") {
    REQUIRE_THROWS_AS(spdlog::sinks::ringbuffer_sink_mt(0), spdlog::spdlog_ex);
}

TEST_CASE("ringbuffer stores formatted messages", "[ringbuffer]") {
    spdlog::sinks::ringbuffer_sink_st sink(3);
    sink.set_pattern("%v");

    sink.log(spdlog::details::log_msg{"test", spdlog::level::info, "msg1"});
    sink.log(spdlog::details::log_msg{"test", spdlog::level::info, "msg2"});
    sink.log(spdlog::details::log_msg{"test", spdlog::level::info, "msg3"});

    auto formatted = sink.last_formatted();
    REQUIRE(formatted.size() == 3);
    REQUIRE(formatted[0] == "msg1");
    REQUIRE(formatted[1] == "msg2");
    REQUIRE(formatted[2] == "msg3");
}

TEST_CASE("ringbuffer overrun keeps last items", "[ringbuffer]") {
    spdlog::sinks::ringbuffer_sink_st sink(2);
    sink.set_pattern("%v");

    sink.log(spdlog::details::log_msg{"test", spdlog::level::info, "first"});
    sink.log(spdlog::details::log_msg{"test", spdlog::level::info, "second"});
    sink.log(spdlog::details::log_msg{"test", spdlog::level::info, "third"});

    auto formatted = sink.last_formatted();
    REQUIRE(formatted.size() == 2);
    REQUIRE(formatted[0] == "second");
    REQUIRE(formatted[1] == "third");
}

TEST_CASE("ringbuffer retrieval limit", "[ringbuffer]") {
    spdlog::sinks::ringbuffer_sink_st sink(3);
    sink.set_pattern("%v");

    sink.log(spdlog::details::log_msg{"test", spdlog::level::info, "A"});
    sink.log(spdlog::details::log_msg{"test", spdlog::level::info, "B"});
    sink.log(spdlog::details::log_msg{"test", spdlog::level::info, "C"});

    auto formatted = sink.last_formatted(2);
    REQUIRE(formatted.size() == 2);
    REQUIRE(formatted[0] == "B");
    REQUIRE(formatted[1] == "C");
}

