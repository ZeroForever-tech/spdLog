#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

#include "../details/async_log_msg.h"
#include "../details/err_helper.h"
#include "sink.h"

// async_sink is a sink that sends log messages to a dist_sink in a separate thread using a queue.
// The worker thread dequeues the messages and sends them to the dist_sink to perform the actual logging.
// Once the sink is destroyed, the worker thread empties the queue and exits.

namespace spdlog::details {  // forward declaration
template <typename T>
class mpmc_blocking_queue;
}

namespace spdlog {
namespace sinks {

class SPDLOG_API async_sink final : public sink {
public:
    enum class overflow_policy : std::uint8_t {
        block,           // Block until the log message can be enqueued (default).
        overrun_oldest,  // Overrun the oldest message in the queue if full.
        discard_new      // Discard the log message if the queue is full
    };

    enum { default_queue_size = 8192, max_queue_size = 10 * 1024 * 1024 };

    struct config {
        size_t queue_size = default_queue_size;
        overflow_policy policy = overflow_policy::block;
        std::vector<std::shared_ptr<sink>> sinks;
        std::function<void()> on_thread_start = nullptr;
        std::function<void()> on_thread_stop = nullptr;
        err_handler custom_err_handler = nullptr;
    };

    explicit async_sink(config async_config);

    // create an async_sink with one backend sink
    template <typename Sink, typename... SinkArgs>
    static std::shared_ptr<async_sink> with(SinkArgs &&...sink_args) {
        config cfg{};
        cfg.sinks.emplace_back(std::make_shared<Sink>(std::forward<SinkArgs>(sink_args)...));
        return std::make_shared<async_sink>(cfg);
    }

    ~async_sink() override;

    // sink interface implementation
    void log(const details::log_msg &msg) override;
    void flush() override;
    void set_pattern(const std::string &pattern) override;
    void set_formatter(std::unique_ptr<formatter> sink_formatter) override;

    // async sink specific methods
    [[nodiscard]] size_t get_overrun_counter() const;
    void reset_overrun_counter() const;
    [[nodiscard]] size_t get_discard_counter() const;
    void reset_discard_counter() const;
    [[nodiscard]] const config &get_config() const;

private:
    using async_log_msg = details::async_log_msg;
    using queue_t = details::mpmc_blocking_queue<async_log_msg>;

    void send_message_(async_log_msg::type msg_type, const details::log_msg &msg) const;
    void backend_loop_();
    void backend_log_(const details::log_msg &msg) ;
    void backend_flush_();

    config config_;
    std::unique_ptr<queue_t> q_;
    std::thread worker_thread_;
    details::err_helper err_helper_;
};

}  // namespace sinks
}  // namespace spdlog
