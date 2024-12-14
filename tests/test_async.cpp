#include <tuple>

#include "includes.h"
#include "spdlog/sinks/async_sink.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "test_sink.h"

#define TEST_FILENAME "test_logs/async_test.log"

using spdlog::sinks::async_sink_mt;
using spdlog::sinks::sink;
using spdlog::sinks::test_sink_mt;

auto creat_async_logger(size_t queue_size, std::shared_ptr<sink> backend_sink) {
    auto async_sink = std::make_shared<async_sink_mt>(queue_size);
    async_sink->add_sink(backend_sink);
    auto logger = std::make_shared<spdlog::logger>("async_logger", async_sink);
    return std::make_tuple(logger, async_sink);
}

TEST_CASE("basic async test ", "[async]") {
    const auto test_sink = std::make_shared<test_sink_mt>();
    size_t overrun_counter = 0;
    const size_t queue_size = 16;
    size_t messages = 256;
    {
        auto [logger, async_sink] = creat_async_logger(queue_size, test_sink);
        for (size_t i = 0; i < messages; i++) {
            logger->info("Hello message #{}", i);
        }
        logger->flush();
        overrun_counter = async_sink->get_overrun_counter();
    }
    // logger and async_sink are destroyed here so the queue should be emptied
    REQUIRE(test_sink->msg_counter() == messages);
    REQUIRE(test_sink->flush_counter() == 1);
    REQUIRE(overrun_counter == 0);
}

TEST_CASE("discard policy ", "[async]") {
    auto test_sink = std::make_shared<test_sink_mt>();
    test_sink->set_delay(std::chrono::milliseconds(1));
    size_t queue_size = 4;
    size_t messages = 1024;

    auto [logger, async_sink] = creat_async_logger(queue_size, test_sink);
    async_sink->set_overflow_policy(async_sink_mt::overflow_policy::overrun_oldest);
    REQUIRE(async_sink->get_overflow_policy() == async_sink_mt::overflow_policy::overrun_oldest);
    REQUIRE(async_sink->get_discard_counter() == 0);
    REQUIRE(async_sink->get_overrun_counter() == 0);
    for (size_t i = 0; i < messages; i++) {
        logger->info("Hello message");
    }
    REQUIRE(test_sink->msg_counter() < messages);
    REQUIRE(async_sink->get_overrun_counter() > 0);
    async_sink->reset_overrun_counter();
    REQUIRE(async_sink->get_overrun_counter() == 0);
}

TEST_CASE("discard policy discard_new ", "[async]") {
    auto test_sink = std::make_shared<test_sink_mt>();
    test_sink->set_delay(std::chrono::milliseconds(1));
    size_t queue_size = 4;
    size_t messages = 1024;

    auto [logger, async_sink] = creat_async_logger(queue_size, test_sink);
    async_sink->set_overflow_policy(async_sink_mt::overflow_policy::discard_new);
    REQUIRE(async_sink->get_overflow_policy() == async_sink_mt::overflow_policy::discard_new);
    REQUIRE(async_sink->get_discard_counter() == 0);
    REQUIRE(async_sink->get_overrun_counter() == 0);
    for (size_t i = 0; i < messages; i++) {
        logger->info("Hello message");
    }
    REQUIRE(test_sink->msg_counter() < messages);
    REQUIRE(async_sink->get_discard_counter() > 0);
    async_sink->reset_discard_counter();
    REQUIRE(async_sink->get_discard_counter() == 0);
}

TEST_CASE("flush", "[async]") {
    auto test_sink = std::make_shared<test_sink_mt>();
    size_t queue_size = 256;
    size_t messages = 256;
    {
        auto [logger, async_sink] = creat_async_logger(queue_size, test_sink);
        for (size_t i = 0; i < messages; i++) {
            logger->info("Hello message #{}", i);
        }

        logger->flush();
    }
    // std::this_thread::sleep_for(std::chrono::milliseconds(250));
    REQUIRE(test_sink->msg_counter() == messages);
    REQUIRE(test_sink->flush_counter() == 1);
}

TEST_CASE("wait_dtor ", "[async]") {
    auto test_sink = std::make_shared<spdlog::sinks::test_sink_mt>();
    test_sink->set_delay(std::chrono::milliseconds(5));
    size_t messages = 100;
    {
        auto [logger, async_sink] = creat_async_logger(messages, test_sink);
        async_sink->set_overflow_policy(async_sink_mt::overflow_policy::block);

        for (size_t i = 0; i < messages; i++) {
            logger->info("Hello message #{}", i);
        }
        logger->flush();
        REQUIRE(async_sink->get_overrun_counter() == 0);
        REQUIRE(async_sink->get_discard_counter() == 0);
    }

    REQUIRE(test_sink->msg_counter() == messages);
    REQUIRE(test_sink->flush_counter() == 1);
}

TEST_CASE("multi threads", "[async]") {
    auto test_sink = std::make_shared<spdlog::sinks::test_sink_mt>();
    size_t queue_size = 128;
    size_t messages = 256;
    size_t n_threads = 10;
    {
        auto [logger, async_sink] = creat_async_logger(queue_size, test_sink);

        std::vector<std::thread> threads;
        for (size_t i = 0; i < n_threads; i++) {
            threads.emplace_back([l = logger, msgs = messages] {
                for (size_t j = 0; j < msgs; j++) {
                    l->info("Hello message #{}", j);
                }
            });
            logger->flush();
        }

        for (auto &t : threads) {
            t.join();
        }
    }

    REQUIRE(test_sink->msg_counter() == messages * n_threads);
    REQUIRE(test_sink->flush_counter() == n_threads);
}

TEST_CASE("to_file", "[async]") {
    prepare_logdir();
    size_t messages = 1024;
    {
        spdlog::filename_t filename = SPDLOG_FILENAME_T(TEST_FILENAME);
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(filename, true);
        auto [logger, async_sink] = creat_async_logger(messages, file_sink);

        for (size_t j = 0; j < messages; j++) {
            logger->info("Hello message #{}", j);
        }
    }
    require_message_count(TEST_FILENAME, messages);
    auto contents = file_contents(TEST_FILENAME);
    using spdlog::details::os::default_eol;
    REQUIRE(ends_with(contents, spdlog::fmt_lib::format("Hello message #1023{}", default_eol)));
}

TEST_CASE("bad_ctor", "[async]") { REQUIRE_THROWS_AS(std::make_shared<async_sink_mt>(0), spdlog::spdlog_ex); }

TEST_CASE("bad_ctor2", "[async]") { REQUIRE_THROWS_AS(std::make_shared<async_sink_mt>(-1), spdlog::spdlog_ex); }

TEST_CASE("start_stop_clbks", "[async]") {
    bool start_called = false;
    bool stop_called = false;
    {
        auto sink = std::make_shared<async_sink_mt>([&] { start_called = true; }, [&] { stop_called = true; });
    }
    REQUIRE(start_called);
    REQUIRE(stop_called);
}

TEST_CASE("start_stop_clbks2", "[async]") {
    bool start_called = false;
    bool stop_called = false;
    {
        auto sink = std::make_shared<async_sink_mt>([&] { start_called = true; }, nullptr);
    }
    REQUIRE(start_called);
    REQUIRE_FALSE(stop_called);
}

TEST_CASE("start_stop_clbks3", "[async]") {
    bool start_called = false;
    bool stop_called = false;
    {
        auto sink = std::make_shared<async_sink_mt>(nullptr, [&] { stop_called = true; });
    }
    REQUIRE_FALSE(start_called);
    REQUIRE(stop_called);
}

TEST_CASE("start_stop_clbks4", "[async]") {
    bool start_called = false;
    bool stop_called = false;
    {
        auto sink = std::make_shared<async_sink_mt>(128, [&] { start_called = true; }, [&] { stop_called = true; });
    }
    REQUIRE(start_called);
    REQUIRE(stop_called);
}

TEST_CASE("start_stop_clbks5", "[async]") {
    bool start_called = false;
    bool stop_called = false;
    {
        REQUIRE_THROWS(std::make_shared<async_sink_mt>(0, [&] { start_called = true; }, [&] { stop_called = true; }));
    }
    REQUIRE_FALSE(start_called);
    REQUIRE_FALSE(stop_called);
}

TEST_CASE("multi-sinks", "[async]") {
    prepare_logdir();
    auto test_sink1 = std::make_shared<spdlog::sinks::test_sink_mt>();
    auto test_sink2 = std::make_shared<spdlog::sinks::test_sink_mt>();
    auto test_sink3 = std::make_shared<spdlog::sinks::test_sink_mt>();
    size_t messages = 1024;
    {
        auto [logger, async_sink] = creat_async_logger(messages, test_sink1);
        async_sink->add_sink(test_sink2);
        async_sink->add_sink(test_sink3);

        for (size_t j = 0; j < messages; j++) {
            logger->info("Hello message #{}", j);
        }
    }
    REQUIRE(test_sink1->msg_counter() == messages);
    REQUIRE(test_sink2->msg_counter() == messages);
    REQUIRE(test_sink3->msg_counter() == messages);
}

TEST_CASE("level-off", "[async]") {
    const auto test_sink = std::make_shared<test_sink_mt>();
    test_sink->set_level(spdlog::level::critical);
    const size_t queue_size = 16;
    size_t messages = 256;
    {
        auto [logger, async_sink] = creat_async_logger(queue_size, test_sink);
        logger->flush_on(spdlog::level::critical);
        for (size_t i = 0; i < messages; i++) {
            logger->info("Hello message #{}", i);
        }
    }

    // logger and async_sink are destroyed here so the queue should be emptied
    REQUIRE(test_sink->msg_counter() == 0);
    REQUIRE(test_sink->flush_counter() == 0);
}
