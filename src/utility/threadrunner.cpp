#include "utility/threadrunner.h"

#include <utility>

#include "utility/log.h"

namespace muonpi {

thread_runner::thread_runner(std::string name, bool use_custom_run)
    : m_use_custom_run { std::move(use_custom_run) }
    , m_name { std::move(name) }
{
}

thread_runner::~thread_runner()
{
    finish();
}

void thread_runner::stop()
{
    m_run = false;
    m_quit = true;
    m_condition.notify_all();
}

void thread_runner::join()
{
    if (m_run_future.valid()) {
        m_run_future.wait();
    }
}

auto thread_runner::step() -> int
{
    return 0;
}

auto thread_runner::pre_run() -> int
{
    return 0;
}

auto thread_runner::post_run() -> int
{
    return 0;
}

auto thread_runner::custom_run() -> int
{
    return 0;
}

auto thread_runner::wait() -> int
{
    if (!m_run_future.valid()) {
        return -1;
    }
    join();
    return m_run_future.get();
}

auto thread_runner::state() -> State
{
    return m_state;
}

auto thread_runner::run() -> int
{
    m_state = State::Initialising;
    struct StateGuard {
        State& state;
        bool clean { false };

        ~StateGuard()
        {
            if (clean) {
                state = State::Stopped;
            } else {
                state = State::Error;
            }
        }
    } guard { m_state };

    Log::debug() << "Starting thread " + m_name;
    int pre_result { pre_run() };
    if (pre_result != 0) {
        return pre_result;
    }
    try {
        m_state = State::Running;
        if (m_use_custom_run) {
            int result { custom_run() };
            if (result != 0) {
                return result;
            }
        } else {
            while (m_run) {
                int result { step() };
                if (result != 0) {
                    Log::warning() << "Thread " + m_name + " Stopped.";
                    return result;
                }
            }
        }
    } catch (std::exception& e) {
        Log::error() << "Thread " + m_name + "Got an uncaught exception: " + std::string { e.what() };
        return -1;
    } catch (...) {
        Log::error() << "Thread " + m_name + "Got an uncaught exception.";
        return -1;
    }
    m_state = State::Finalising;
    Log::debug() << "Stopping thread " + m_name;
    guard.clean = true;
    return post_run();
}

void thread_runner::finish()
{
    stop();
    join();
}

auto thread_runner::name() -> std::string
{
    return m_name;
}

auto thread_runner::state_string() -> std::string
{
    switch (m_state) {
    case State::Error:
        return "Error";
    case State::Stopped:
        return "Stopped";
    case State::Initial:
        return "Initial";
    case State::Initialising:
        return "Initialising";
    case State::Running:
        return "Running";
    case State::Finalising:
        return "Finalising";
    }
    return {};
}

void thread_runner::start()
{
    if (m_state > State::Initial) {
        Log::info() << "Thread " + m_name + " already running, refusing to start.";
        return;
    }
    m_run_future = std::async(std::launch::async, &thread_runner::run, this);
}

void thread_runner::start_synchronuos()
{
    if (m_state > State::Initial) {
        return;
    }
    std::promise<int> promise {};
    m_run_future = promise.get_future();
    int value = run();
    promise.set_value(value);
}

}
