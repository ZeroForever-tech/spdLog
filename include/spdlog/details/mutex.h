// SPDX-License-Identifier: MIT
// Copyright(c) spdlog contributors

// Mutexes used to help support priority inheritance support
//
// Standard library implementations will typically create mutexes that do
// not take into consideration the priority and scheduling of the thread
// they claim ownership on. In (near) real-time implementations, this is
// not ideal since this may lead to priority inversion scenarios.
//
// The following provides mutex implementations that can help avoid priority
// inversion scenarios by ensuring the underlying mutexes used take advantage
// of priority inheritance protocols. spdlog uses `std::mutex` and
// `std::recursive_mutex`, which are typically not priority inversion "safe".
// Therefore, we introduces custom mutexes for them below. Calls such as
// `std::condition_variable` are fine if the managed mutex used by the
// `std::unique_lock` are safe. However, the use of
// `std::condition_variable_any` is not due their implementations relying on
// both a provided mutex and an internal `std::mutex`. This is why the
// implementation opts for using `std::condition_variable` with the help of
// `mtx()` methods over using `std::condition_variable_any`.

#pragma once

#include <mutex>
#include <spdlog/common.h>

namespace spdlog {
namespace details {


class spdlog_mutex {
public:
    spdlog_mutex() {
#ifdef SPDLOG_PRIORITY_INHERITANCE
#if defined(_GLIBCXX_HAS_GTHREADS) || defined(_LIBCPP_HAS_THREAD_API_PTHREAD)
        pthread_mutexattr_t attr;
        int ret = pthread_mutexattr_init(&attr);
        if (ret != 0) {
            return;
        }
   
        ret = pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT);
        if (ret != 0) {
            return;
        }

        auto handle = mtx_.native_handle();
        ret = pthread_mutex_destroy(handle);
        if (ret < 0) {
            throw_spdlog_ex("failed to tear down mutex", ret);
        }

        ret = pthread_mutex_init(handle, &attr);
        if (ret < 0) {
            throw_spdlog_ex("failed to build mutex", ret);
        }
#else
    #error Priority inheritance is not supported.
#endif
#endif
    }

    void lock() {
        mtx_.lock();
    }

    bool try_lock() {
        return mtx_.try_lock();
    }

    void unlock() {
        mtx_.unlock();
    }

    std::mutex& mtx() {
        return mtx_;
    }

private:
    std::mutex mtx_;
};


class spdlog_recursive_mutex {
public:
    spdlog_recursive_mutex() {
#ifdef SPDLOG_PRIORITY_INHERITANCE
#if defined(_GLIBCXX_HAS_GTHREADS) || defined(_LIBCPP_HAS_THREAD_API_PTHREAD)
        pthread_mutexattr_t attr;
        int ret = pthread_mutexattr_init(&attr);
        if (ret != 0) {
            return;
        }

        ret = pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT);
        if (ret != 0) {
            return;
        }

        ret = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        if (ret != 0) {
            return;
        }

        auto handle = mtx_.native_handle();
        ret = pthread_mutex_destroy(handle);
        if (ret < 0) {
            throw_spdlog_ex("failed to tear down mutex", ret);
        }

        ret = pthread_mutex_init(handle, &attr);
        if (ret < 0) {
            throw_spdlog_ex("failed to build mutex", ret);
        }
#else
    #error Priority inheritance is not supported.
#endif
#endif
    }

    void lock() {
        mtx_.lock();
    }

    bool try_lock() {
        return mtx_.try_lock();
    }

    void unlock() {
        mtx_.unlock();
    }

    std::recursive_mutex& mtx() {
        return mtx_;
    }

private:
    std::recursive_mutex mtx_;
};


}  // namespace details
}  // namespace spdlog
