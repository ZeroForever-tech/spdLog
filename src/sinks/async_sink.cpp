// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#include "spdlog/sinks/async_sink.h"

#include <memory>
#include <mutex>
#include <cassert>

#include "spdlog/details/mpmc_blocking_q.h"
#include "spdlog/common.h"
#include "spdlog/pattern_formatter.h"

namespace spdlog {
namespace sinks {

template <typename Mutex>
async_sink<Mutex>::async_sink(size_t queue_size, std::function<void()> on_thread_start, std::function<void()> on_thread_stop)
    : base_t() {
    if (queue_size == 0 || queue_size > max_queue_size) {
        throw spdlog_ex("async_sink: invalid queue size");
    }
    // printf("........... Allocating queue: slot: %zu X %zu bytes ====> %lld KB ..............\n",
    //   queue_size, sizeof(details::async_log_msg), (sizeof(details::async_log_msg) * queue_size)/1024);
    q_ = std::make_unique<queue_t>(queue_size);

    worker_thread_ = std::thread([this, on_thread_start, on_thread_stop] {
        if (on_thread_start) on_thread_start();
        this->backend_loop_();
        if (on_thread_stop) on_thread_stop();
    });
}

template <typename Mutex>
async_sink<Mutex>::~async_sink() {
    try {
        q_->enqueue(async_log_msg(async_log_msg::type::terminate));
        worker_thread_.join();
    } catch (...) {
    }
};

template <typename Mutex>
async_sink<Mutex>::async_sink()
    : async_sink(default_queue_size, nullptr, nullptr) {}

template <typename Mutex>
async_sink<Mutex>::async_sink(size_t queue_size)
    : async_sink(queue_size, nullptr, nullptr) {}

template <typename Mutex>
async_sink<Mutex>::async_sink(std::function<void()> on_thread_start, std::function<void()> on_thread_stop)
    : async_sink(default_queue_size, on_thread_start, on_thread_stop) {}

template <typename Mutex>
void async_sink<Mutex>::sink_it_(const details::log_msg &msg) {
    send_message_(async_log_msg::type::log, msg);
}


template <typename Mutex>
void async_sink<Mutex>::set_overflow_policy(overflow_policy policy) {
    overflow_policy_ = policy;
}

template <typename Mutex>
typename async_sink<Mutex>::overflow_policy async_sink<Mutex>::get_overflow_policy() const {
    return overflow_policy_;
}

template <typename Mutex>
size_t async_sink<Mutex>::get_overrun_counter() const {
    return q_->overrun_counter();
}

template <typename Mutex>
void async_sink<Mutex>::reset_overrun_counter() const {
    q_->reset_overrun_counter();
}

template <typename Mutex>
size_t async_sink<Mutex>::get_discard_counter() const {
    return q_->discard_counter();
}

template <typename Mutex>
void async_sink<Mutex>::reset_discard_counter() const {
    q_->reset_discard_counter();
}

template <typename Mutex>
void async_sink<Mutex>::flush_() {
    send_message_(async_log_msg::type::flush, details::log_msg());
}

template <typename Mutex>
void async_sink<Mutex>::send_message_(async_log_msg::type msg_type, const details::log_msg &msg) {
    switch (overflow_policy_) {
        case overflow_policy::block:
            q_->enqueue(async_log_msg(msg_type, msg));
            break;
        case overflow_policy::overrun_oldest:
            q_->enqueue_nowait(async_log_msg(msg_type, msg));
            break;
        case overflow_policy::discard_new:
            q_->enqueue_if_have_room(async_log_msg(msg_type, msg));
            break;
        default:
            assert(false);
            throw spdlog_ex("async_sink: invalid overflow policy");
    }
}

template <typename Mutex>
void async_sink<Mutex>::backend_loop_() {
    details::async_log_msg incoming_msg;
    for (;;) {
        q_->dequeue(incoming_msg);
        switch (incoming_msg.message_type()) {
            case async_log_msg::type::log:
                base_t::sink_it_(incoming_msg);
                break;
            case async_log_msg::type::flush:
                base_t::flush_();
                break;
            case async_log_msg::type::terminate:
                return;
            default:
                assert(false);
        }
    }
}

}  // namespace sinks
}  // namespace spdlog

// template instantiations
#include "spdlog/details/null_mutex.h"
template class SPDLOG_API spdlog::sinks::async_sink<std::mutex>;
template class SPDLOG_API spdlog::sinks::async_sink<spdlog::details::null_mutex>;
