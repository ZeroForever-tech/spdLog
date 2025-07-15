//
// Copyright(c) 2015 Gabi Melman.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)
//

//
// bench.cpp : spdlog benchmarks
//
#include <algorithm>
#include <atomic>
#include <fstream>
#include <iostream>
#include <locale>
#include <memory>
#include <string>
#include <thread>

#include "spdlog/sinks/async_sink.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/spdlog.h"

using namespace std;
using namespace std::chrono;
using spdlog::sinks::async_sink;

void bench_mt(int howmany, std::shared_ptr<spdlog::logger> log, int thread_count);

/**
 * @brief Counts the number of lines in a given file.
 * @example
 * int lineCount = count_lines("path/to/file.txt");
 * std::cout << lineCount << std::endl; // Expected output: number of lines in the file
 * 
 * @param {char*} filename - Path to the file whose lines are to be counted.
 * @return {int} - The total number of lines in the specified file.
 * 
 * @details
 *   - Assumes the file can be opened and read without errors.
 *   - Utilizes std::count to iterate and count newline characters.
 */
int count_lines(const char *filename) {
    std::ifstream ifs(filename);
    return std::count(std::istreambuf_iterator(ifs), std::istreambuf_iterator<char>(), '\n');
}

/**
 * Verifies that a specified file contains a specific number of lines
 * and logs the process.
 *
 * @param {string} filename - The name of the file to verify.
 * @param {number} expected_count - The expected line count of the file.
 * @example
 * verify_file('example.txt', 10);
 * // Logs information and exits if the file does not contain 10 lines.
 *
 * @returns {void}
 *
 * @details
 *  - If the actual line count does not match the expected line count, logs an error and terminates the program.
 *  - Uses a logging library to inform about verification steps.
 *  - The function exits with status code 1 on failure.
 */
void verify_file(const char *filename, int expected_count) {
    spdlog::info("Verifying {} to contain {} line..", filename, expected_count);
    auto count = count_lines(filename);
    if (count != expected_count) {
        spdlog::error("Test failed. {} has {} lines instead of {}", filename, count, expected_count);
        exit(1);
    }
    spdlog::info("Line count OK ({})\n", count);
}

#ifdef _MSC_VER
    #pragma warning(pop)
#endif

using namespace spdlog::sinks;

/**
 * @function main
 * @description Entry point for the async logger benchmarking program which evaluates the performance of logging a specified number of messages using asynchronous logging with different queue overflow policies.
 * @param {number} argc - Count of command-line arguments, including the program's name.
 * @param {Array<string>} argv - Array of command-line arguments. Can include message count, thread count, queue size, and number of iterations.
 * @returns {number} - Returns 0 on successful execution, 1 otherwise if there is an error or invalid input.
 * @example
 * main( ["1000", "4", "1024", "3"] );
 * // Expected output: Benchmark results logged to console or file with the specified configuration.
 * 
 * @details
 *   - Validates input values to ensure none of the numeric arguments are below 1.
 *   - Compares queue size against the maximum allowed and exits if it exceeds.
 *   - Two queue overflow policies are tested: block and overrun, each in separate iterations.
 *   - Logs performance results in terms of number of messages, threads, queue slots, iteration count, and queue memory consumption.
 */
int main(int argc, char *argv[]) {
    // setlocale to show thousands separators
    std::locale::global(std::locale("en_US.UTF-8"));
    int howmany = 1'000'000;
    int queue_size = async_sink::default_queue_size;
    int threads = 10;
    int iters = 3;

    try {
        spdlog::set_pattern("[%^%l%$] %v");
        if (argc > 1 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
            spdlog::info("Usage: {} <message_count> <threads> <q_size> <iterations>", argv[0]);
            return 0;
        }

        if (argc > 1) howmany = atoi(argv[1]);
        if (argc > 2) threads = atoi(argv[2]);
        if (argc > 3) {
            queue_size = atoi(argv[3]);
        }

        if (argc > 4) iters = atoi(argv[4]);
        // validate all argc values
        if (howmany < 1 || threads < 1 || queue_size < 1 ||  iters < 1) {
            spdlog::error("Invalid input values");
            exit(1);
        }

        constexpr int max_q_size = async_sink::max_queue_size;
        if(queue_size > max_q_size)
        {
            spdlog::error("Queue size too large. Max queue size is {:L}", max_q_size);
            exit(1);
        }

        auto slot_size = sizeof(spdlog::details::async_log_msg);
        spdlog::info("-------------------------------------------------");
        spdlog::info("Messages     : {:L}", howmany);
        spdlog::info("Threads      : {:L}", threads);
        spdlog::info("Queue        : {:L} slots", queue_size);
        spdlog::info("Queue memory : {:L} x {:L} = {:L} KB ", queue_size, slot_size, (queue_size * slot_size) / 1024);
        spdlog::info("Total iters  : {:L}", iters);
        spdlog::info("-------------------------------------------------");

        const char *filename = "logs/basic_async.log";
        spdlog::info("");
        spdlog::info("*********************************");
        spdlog::info("Queue Overflow Policy: block");
        spdlog::info("*********************************");
        for (int i = 0; i < iters; i++) {
            {
                auto file_sink = std::make_shared<basic_file_sink_mt>(filename, true);
                auto cfg = async_sink::config();
                cfg.queue_size = queue_size;
                cfg.sinks.push_back(std::move(file_sink));
                auto sink = std::make_shared<async_sink>(cfg);
                auto logger = std::make_shared<spdlog::logger>("async_logger", std::move(sink));
                bench_mt(howmany, std::move(logger), threads);
            }
            // verify_file(filename, howmany); // in separate scope to ensure logger is destroyed and all logs were written
        }
        spdlog::info("");
        spdlog::info("*********************************");
        spdlog::info("Queue Overflow Policy: overrun");
        spdlog::info("*********************************");
        // do same test but discard the oldest if queue is full instead of blocking
        filename = "logs/basic_async-overrun.log";
        for (int i = 0; i < iters; i++) {
            async_sink::config cfg;
            cfg.policy = async_sink::overflow_policy::overrun_oldest;
            cfg.queue_size = queue_size;
            auto file_sink = std::make_shared<basic_file_sink_mt>(filename, true);
            cfg.sinks.push_back(std::move(file_sink));
            auto sink = std::make_shared<async_sink>(cfg);
            auto logger = std::make_shared<spdlog::logger>("async_logger", std::move(sink));
            bench_mt(howmany, std::move(logger), threads);
        }
        spdlog::shutdown();
    } catch (std::exception &ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        perror("Last error");
        return 1;
    }
    return 0;
}

/**
 * Logs a specified number of messages using a shared logger instance.
 *
 * @function thread_fun
 * @param {Object} logger - A shared pointer to a spdlog logger object used for logging messages.
 * @param {number} howmany - The number of messages to log.
 * 
 * @example
 * const logger = spdlog::stdout_color_mt("console");
 * thread_fun(logger, 10);
 * // Logs 10 messages to the console.
 *
 * @details
 * - This function uses the logger provided to record messages sequentially.
 * - Each message includes an index number to differentiate between logged messages.
 */
void thread_fun(std::shared_ptr<spdlog::logger> logger, int howmany) {
    for (int i = 0; i < howmany; i++) {
        logger->info("Hello logger: msg number {}", i);
    }
}

/**
 * Executes a multi-threaded benchmark for logging.
 *
 * This function creates multiple threads to perform logging operations 
 * using the specified logger instance, aimed to measure the execution 
 * time for logging a specified number of messages.
 *
 * @param {int} howmany - The total number of messages to be logged.
 * @param {std::shared_ptr<spdlog::logger>} logger - The logger instance used for logging operations.
 * @param {int} thread_count - The number of threads to be spawned for concurrent logging.
 * 
 * @example
 * bench_mt(100000, my_logger, 4);
 * 
 * @details
 *   - The function distributes the logging workload evenly across the specified number of threads.
 *   - The execution time and logging rate are outputted via spdlog::info at completion.
 *   - If the `howmany` is not perfectly divisible by `thread_count`, the first thread will handle the remainder.
 */
void bench_mt(int howmany, std::shared_ptr<spdlog::logger> logger, int thread_count) {
    using std::chrono::high_resolution_clock;
    vector<std::thread> threads;
    auto start = high_resolution_clock::now();

    int msgs_per_thread = howmany / thread_count;
    int msgs_per_thread_mod = howmany % thread_count;
    for (int t = 0; t < thread_count; ++t) {
        if (t == 0 && msgs_per_thread_mod)
            threads.push_back(std::thread(thread_fun, logger, msgs_per_thread + msgs_per_thread_mod));
        else
            threads.push_back(std::thread(thread_fun, logger, msgs_per_thread));
    }

    for (auto &t : threads) {
        t.join();
    }

    auto delta = high_resolution_clock::now() - start;
    auto delta_d = duration_cast<duration<double>>(delta).count();
    spdlog::info("Elapsed: {} secs\t {:L}/sec", delta_d, static_cast<int>(howmany / delta_d));
}
