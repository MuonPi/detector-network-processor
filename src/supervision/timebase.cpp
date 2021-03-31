#include "supervision/timebase.h"

#include "messages/event.h"

#include "utility/log.h"

#include <algorithm>

namespace muonpi {

TimebaseSupervisor::TimebaseSupervisor(sink::base<Event>& event_sink, sink::base<Timebase>& timebase_sink)
    : pipeline<Event> { event_sink }
    , pipeline<Timebase> { timebase_sink }
{
}

void TimebaseSupervisor::get(Event event)
{
    if (event.start() < m_start) {
        m_start = event.start();
    } else if (event.start() > m_end) {
        m_end = event.start();
    }
    pipeline<Event>::put(event);
}

void TimebaseSupervisor::get(Timebase timebase)
{
    if ((std::chrono::system_clock::now() - m_sample_start) < s_sample_time) {
        timebase.base = m_current;
        pipeline<Timebase>::put(timebase);
        return;
    }

    m_sample_start = std::chrono::system_clock::now();

    m_current = std::clamp(std::chrono::nanoseconds { m_end - m_start }, s_minimum, s_maximum);

    timebase.base = m_current;

    pipeline<Timebase>::put(timebase);

    m_start = std::numeric_limits<std::int_fast64_t>::max();
    m_end = std::numeric_limits<std::int_fast64_t>::min();
}
}
