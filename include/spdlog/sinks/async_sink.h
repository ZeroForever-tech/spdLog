#pragma once

#include <cstdint>
#include <thread>
#include <atomic>
#include <functional>

#include "../details/async_log_msg.h"
#include "../details/mpmc_blocking_q.h"
#include "dist_sink.h"

// async_sink is a sink that sends log messages to a dist_sink in a separate thread using a queue.
// The worker thread dequeues the messages and sends them to the dist_sink to perform the actual logging.
// The worker thread is terminated when the async_sink is destroyed.

namespace spdlog {
namespace sinks {

template <typename Mutex>
class async_sink final : public dist_sink<Mutex> {
public:
    using base_t = dist_sink<Mutex>;
    using async_log_msg = details::async_log_msg;
    using queue_t = details::mpmc_blocking_queue<async_log_msg>;
    enum { default_queue_size = 8192, max_queue_size = 1024 * 1024 * 10 };

    // Async overflow policy - block by default.
    enum class overflow_policy : std::uint8_t {
        block,           // Block until the log message can be enqueued (default).
        overrun_oldest,  // Overrun the oldest message in the queue if full.
        discard_new      // Discard the log message if the queue is full
    };

    async_sink(size_t queue_size, std::function<void()> on_thread_start, std::function<void()> on_thread_stop);
    ~async_sink() override;

    async_sink();
    explicit async_sink(size_t queue_size);
    async_sink(std::function<void()> on_thread_start, std::function<void()> on_thread_stop);
    async_sink(const async_sink &) = delete;
    async_sink &operator=(const async_sink &) = delete;
    async_sink(async_sink &&) = default;
    async_sink &operator=(async_sink &&) = default;

    void set_overflow_policy(overflow_policy policy);
    [[nodiscard]] overflow_policy get_overflow_policy() const;

    [[nodiscard]] size_t get_overrun_counter() const;
    void reset_overrun_counter() const;

    [[nodiscard]] size_t get_discard_counter() const;
    void reset_discard_counter() const;

private:
    void sink_it_(const details::log_msg &msg) override;
    void flush_() override;
    void send_message_(const async_log_msg::type msg_type, const details::log_msg &msg);
    void worker_loop();

    std::atomic<overflow_policy> overflow_policy_ = overflow_policy::block;
    std::unique_ptr<queue_t> q_;
    std::thread worker_thread_;
};

using async_sink_mt = async_sink<std::mutex>;
using async_sink_st = async_sink<details::null_mutex>;

}  // namespace sinks

class logger;
template <typename... SinkArgs>
std::shared_ptr<logger> create_async(std::string logger_name, SinkArgs &&...sink_args) {
    auto async_sink = std::make_shared<sinks::async_sink_mt>(std::forward<SinkArgs>(sink_args)...);
    return std::make_shared<logger>(std::move(logger_name), std::move(async_sink));
}
}  // namespace spdlog
