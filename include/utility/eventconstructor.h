#ifndef EVENTCONSTRUCTOR_H
#define EVENTCONSTRUCTOR_H

#include "messages/event.h"

#include <chrono>
#include <memory>

namespace muonpi {

class event_t;
class criterion;
/**
 * @brief The event_constructor class
 */
class event_constructor {
public:
    enum class Type {
        NoMatch,
        Contested,
        Match
    };

    /**
     * @brief set_timeout Set a new timeout for the event_constructor. Only accepts longer timeouts.
     * @param timeout The timeout to set.
     */
    void set_timeout(std::chrono::system_clock::duration timeout);

    /**
     * @brief timed_out Check whether the timeout has been reached
     * @return true if the constructor timed out.
     */
    [[nodiscard]] auto timed_out() const -> bool;

    event_t event;
    std::chrono::system_clock::duration timeout { std::chrono::minutes { 1 } };

private:
    std::chrono::system_clock::time_point m_start { std::chrono::system_clock::now() };
};

}

#endif // EVENTCONSTRUCTOR_H
