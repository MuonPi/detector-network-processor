#ifndef SINKBASE_H
#define SINKBASE_H

#include "utility/threadrunner.h"
#include <array>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace MuonPi::Sink {

template <typename T>
/**
 * @brief The Sink class
 * Represents a canonical Sink for items of type T.
 */
class Base
{
public:
    /**
     * @brief ~Sink The destructor. If this gets called while the event loop is still running, it will tell the loop to finish and wait for it to be done.
     */
    virtual ~Base();

    /**
     * @brief get pushes an item into the sink
     * @param item The item to push
     */
    virtual void get(T item) = 0;
};

template <typename T>
class Threaded : public Base<T>, public ThreadRunner
{
public:
    /**
     * @brief Threaded
     * @param name The name of the thread. Useful for identification.
     * @param timeout The timeout for how long the thread should wait before calling the process method without parameter.
     */
    Threaded(const std::string& name, std::chrono::milliseconds timeout);

    /**
     * @brief Threaded
     * @param name The name of the thread. Useful for identification.
     */
    Threaded(const std::string& name);

    virtual ~Threaded() override;

    /**
     * @brief get Reimplemented from Base<T>. gets called when a new item is available
     * @param item The item that is available
     */
    void get(T item) override;

protected:
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
class Collection : public Threaded<T>
{
public:
    /**
     * @brief Collection A collection of multiple sinks
     * @param sinks The sinks where the items should be distributed
     */
    Collection(std::array<Base<T>*, N> sinks);

    ~Collection() override;

protected:
    /**
     * @brief get Reimplemented from Threaded<T>. gets called when a new item is available
     * @param item The item that is available
     * @return the status code. @see Threaded::process
     */
    [[nodiscard]] auto process(T item) -> int override;

private:
    std::array<Base<T>*, N> m_sinks {};
};



template <typename T>
Base<T>::~Base() = default;


template <typename T>
Threaded<T>::Threaded(const std::string& name)
    : ThreadRunner { name }
{
    start();
}

template <typename T>
Threaded<T>::Threaded(const std::string& name, std::chrono::milliseconds timeout)
    : ThreadRunner { name }
    , m_timeout { timeout }
{
    start();
}

template <typename T>
Threaded<T>::~Threaded() = default;

template <typename T>
void Threaded<T>::get(T item)
{
    std::scoped_lock<std::mutex> lock { m_mutex };
    m_items.push(item);
    m_has_items.notify_all();
}

template <typename T>
auto Threaded<T>::step() -> int
{
    std::mutex mx;
    std::unique_lock<std::mutex> wait_lock { mx };
    if (m_has_items.wait_for(wait_lock, m_timeout ) == std::cv_status::timeout) {
        return process();
    }

    std::size_t n { 0 };
    do {
        auto item = [this] () -> T {
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
        m_has_items.notify_all();
    }
    return process();
}

template <typename T>
auto Threaded<T>::process() -> int
{
    return 0;
}

template <typename T, std::size_t N>
Collection<T, N>::Collection(std::array<Base<T>*, N> sinks)
    : Threaded<T> { "SinkCollection" }
    , m_sinks { std::move(sinks) }
{
}

template <typename T, std::size_t N>
Collection<T, N>::~Collection() = default;


template <typename T, std::size_t N>
auto Collection<T, N>::process(T item) -> int
{
    for (auto* sink : m_sinks) {
        sink->get(item);
    }
    return 0;
}



}

#endif // SINKBASE_H
