/*
 * This content is released under the MIT License as specified in
 * https://raw.githubusercontent.com/gabime/spdlog/v2.x/LICENSE
 */
#include <iostream>

#include "includes.h"
#include "spdlog/sinks/basic_file_sink.h"

static spdlog::filename_t log_filename = SPDLOG_FILENAME_T("test_logs/simple_log.txt");
static std::string log_err_msg = "Error during log";
static std::string flush_err_msg = "Error during flush";

class failing_sink final : public spdlog::sinks::base_sink<std::mutex> {
protected:
    void sink_it_(const spdlog::details::log_msg &) override { throw std::runtime_error(log_err_msg.c_str()); }
    void flush_() override { throw std::runtime_error(flush_err_msg.c_str()); }
};
struct custom_ex {};


using namespace spdlog::sinks;
TEST_CASE("default_error_handler", "[errors]") {
    prepare_logdir();
    auto logger = spdlog::create<basic_file_sink_mt>("test-error", log_filename);
    logger->set_pattern("%v");
    logger->info(SPDLOG_FMT_RUNTIME("Test message {} {}"), 1);
    logger->info("Test message {}", 2);
    logger->flush();
    using spdlog::details::os::default_eol;
    REQUIRE(file_contents(log_filename) == spdlog::fmt_lib::format("Test message 2{}", default_eol));
    REQUIRE(count_lines(log_filename) == 1);
}

TEST_CASE("custom_error_handler", "[errors]") {
    prepare_logdir();
    auto logger = spdlog::create<basic_file_sink_mt>("test-error", log_filename);
    logger->flush_on(spdlog::level::info);
    logger->set_error_handler([=](const std::string & msg) {
        REQUIRE(msg == "argument not found");
        throw custom_ex();
    });
    logger->info("Good message #1");
    REQUIRE_THROWS_AS(logger->info(SPDLOG_FMT_RUNTIME("Bad format msg {} {}"), "xxx"), custom_ex);
    logger->info("Good message #2");
    require_message_count(log_filename, 2);
}

TEST_CASE("default_error_handler2", "[errors]") {
    auto logger = std::make_shared<spdlog::logger>("failed_logger", std::make_shared<failing_sink>());
    logger->set_error_handler([=](const std::string &msg) {
        REQUIRE(msg == log_err_msg);
        throw custom_ex();
    });
    REQUIRE_THROWS_AS(logger->info("Some message"), custom_ex);
}

TEST_CASE("flush_error_handler", "[errors]") {
    auto logger = spdlog::create<failing_sink>("failed_logger");
    logger->set_error_handler([=](const std::string &msg) {
        REQUIRE(msg == flush_err_msg);
        throw custom_ex();
    });
    REQUIRE_THROWS_AS(logger->flush(), custom_ex);
}
