#ifndef TIMEBASESUPERVISOR_H
#define TIMEBASESUPERVISOR_H

#include "pipeline.h"

#include "messages/event.h"

#include <chrono>
#include <memory>

namespace muonpi {

/**
 * @brief The TimebaseSupervisor class
 */
class TimebaseSupervisor : public pipeline<Event>, public pipeline<Timebase> {
public:
    TimebaseSupervisor(sink::base<Event>& event_sink, sink::base<Timebase>& timebase_sink);

    void get(Event event) override;
    void get(Timebase timebase) override;

private:
    static constexpr std::chrono::system_clock::duration s_minimum { std::chrono::milliseconds { 800 } };
    static constexpr std::chrono::system_clock::duration s_maximum { std::chrono::minutes { 2 } };
    static constexpr std::chrono::system_clock::duration s_sample_time { std::chrono::seconds { 2 } };

    std::chrono::system_clock::time_point m_sample_start { std::chrono::system_clock::now() };

    std::int_fast64_t m_start { std::numeric_limits<std::int_fast64_t>::max() };
    std::int_fast64_t m_end { std::numeric_limits<std::int_fast64_t>::min() };

    std::chrono::system_clock::duration m_current { s_minimum };
};

}

#endif // TIMEBASESUPERVISOR_H
