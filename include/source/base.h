#ifndef ABSTRACTEVENTSOURCE_H
#define ABSTRACTEVENTSOURCE_H

#include "utility/threadrunner.h"
#include "sink/base.h"

#include <future>
#include <memory>
#include <atomic>
#include <queue>

namespace MuonPi::Source {

template <typename T>
/**
 * @brief The Base class
 * Represents a canonical Source for items of type T.
 */
class Base
{
public:
    Base(Sink::Base<T>& sink);

    /**
     * @brief ~Base The destructor. If this gets called while the event loop is still running, it will tell the loop to finish and wait for it to be done.
     */
    virtual ~Base();

protected:
    /**
     * @brief put pushes an item into the source
     * @param item The item to push
     */
    void put(T item);

private:
    Sink::Base<T>& m_sink;

};

template <typename T>
Base<T>::Base(Sink::Base<T>& sink)
    : m_sink { sink }
{}

template <typename T>
Base<T>::~Base() = default;

template <typename T>
void Base<T>::put(T item)
{
    m_sink.get(std::move(item));
}


}

#endif // ABSTRACTEVENTSOURCE_H
