#ifndef SINKBASE_H
#define SINKBASE_H

#include "utility/threadrunner.h"
#include <array>
#include <condition_variable>
#include <mutex>
#include <queue>

namespace muonpi::sink {

template <typename T>
/**
 * @brief The sink class
 * Represents a canonical sink for items of type T.
 */
class base {
public:
    /**
     * @brief ~sink The destructor. If this gets called while the event loop is still running, it will tell the loop to finish and wait for it to be done.
     */
    virtual ~base();

    /**
     * @brief get pushes an item into the sink
     * @param item The item to push
     */
    virtual void get(T item) = 0;
};

template <typename T>
class threaded : public base<T>, public ThreadRunner {
public:
    /**
     * @brief threaded
     * @param name The name of the thread. Useful for identification.
     * @param timeout The timeout for how long the thread should wait before calling the process method without parameter.
     */
    threaded(const std::string& name, std::chrono::milliseconds timeout);

    /**
     * @brief threaded
     * @param name The name of the thread. Useful for identification.
     */
    threaded(const std::string& name);

    virtual ~threaded() override;

protected:
    /**
     * @brief internal_get
     * @param item The item that is available
     */
    void internal_get(T item);

    /**
     * @brief step Reimplemented from ThreadRunner.
     * Internally this uses the timeout given in the constructor, default is 5 seconds.
     * It waits for a maximum of timeout, if there is no item available,
     * it calls the process method without parameter, if yes it calls the overloaded process method with the item as parameter.
     * @return the return code of the process methods. If it is nonzero, the Thread will finish.
     */
    [[nodiscard]] auto step() -> int override;

    /**
     * @brief process Gets called whenever a new item is available.
     * @param item The next available item
     * @return The status code which is passed along by the step method.
     */
    [[nodiscard]] virtual auto process(T item) -> int = 0;
    /**
     * @brief process Gets called periodically whenever there is an item available.
     * @return The status code which is passed along by the step method.
     */
    [[nodiscard]] virtual auto process() -> int;

private:
    std::chrono::milliseconds m_timeout { std::chrono::seconds { 5 } };
    std::queue<T> m_items {};
    std::mutex m_mutex {};
    std::condition_variable m_has_items {};
};

template <typename T, std::size_t N>
class collection : public threaded<T> {
public:
    /**
     * @brief collection A collection of multiple sinks
     * @param sinks The sinks where the items should be distributed
     */
    collection(std::array<base<T>*, N> sinks);

    ~collection() override;

    void get(T item) override;

protected:
    /**
     * @brief get Reimplemented from threaded<T>. gets called when a new item is available
     * @param item The item that is available
     * @return the status code. @see threaded::process
     */
    [[nodiscard]] auto process(T item) -> int override;

private:
    std::array<base<T>*, N> m_sinks {};
};

template <typename T>
base<T>::~base() = default;

template <typename T>
threaded<T>::threaded(const std::string& name)
    : ThreadRunner { name }
{
    start();
}

template <typename T>
threaded<T>::threaded(const std::string& name, std::chrono::milliseconds timeout)
    : ThreadRunner { name }
    , m_timeout { timeout }
{
    start();
}

template <typename T>
threaded<T>::~threaded() = default;

template <typename T>
void threaded<T>::internal_get(T item)
{
    std::scoped_lock<std::mutex> lock { m_mutex };
    m_items.push(item);
    m_condition.notify_all();
}

template <typename T>
auto threaded<T>::step() -> int
{
    std::mutex mx;
    std::unique_lock<std::mutex> wait_lock { mx };
    if ((m_items.empty()) && (m_condition.wait_for(wait_lock, m_timeout) == std::cv_status::timeout)) {
        return process();
    }
    if (m_quit) {
        return 0;
    }

    std::size_t n { 0 };
    do {
        auto item = [this]() -> T {
            std::scoped_lock<std::mutex> lock { m_mutex };
            T res = m_items.front();
            m_items.pop();
            return res;
        }();

        int result { process(item) };
        if (result != 0) {
            return result;
        }
        n++;
    } while (!m_items.empty() && (n < 10));
    if (!m_items.empty()) {
        m_condition.notify_all();
    }
    return process();
}

template <typename T>
auto threaded<T>::process() -> int
{
    return 0;
}

template <typename T, std::size_t N>
collection<T, N>::collection(std::array<base<T>*, N> sinks)
    : threaded<T> { "sinkcollection" }
    , m_sinks { std::move(sinks) }
{
}

template <typename T, std::size_t N>
collection<T, N>::~collection() = default;

template <typename T, std::size_t N>
void collection<T, N>::get(T item)
{
    threaded<T>::internal_get(std::move(item));
}

template <typename T, std::size_t N>
auto collection<T, N>::process(T item) -> int
{
    for (auto* sink : m_sinks) {
        sink->get(item);
    }
    return 0;
}

}

#endif // SINKBASE_H
