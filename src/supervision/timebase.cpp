#include "supervision/timebase.h"

#include "messages/event.h"

#include "utility/log.h"

#include <algorithm>

namespace muonpi {

TimeBaseSupervisor::TimeBaseSupervisor(Sink::Base<Event>& event_sink, Sink::Base<TimeBase>& timebase_sink)
    : Pipeline<Event> { event_sink }
    , Pipeline<TimeBase> { timebase_sink }
{
}

void TimeBaseSupervisor::get(Event event)
{
    if (event.start() < m_start) {
        m_start = event.start();
    } else if (event.start() > m_end) {
        m_end = event.start();
    }
    Pipeline<Event>::put(event);
}

void TimeBaseSupervisor::get(TimeBase timebase)
{
    if ((std::chrono::system_clock::now() - m_sample_start) < s_sample_time) {
        timebase.base = m_current;
        Pipeline<TimeBase>::put(timebase);
        return;
    }

    m_sample_start = std::chrono::system_clock::now();

    m_current = std::clamp(std::chrono::nanoseconds { m_end - m_start }, s_minimum, s_maximum);

    timebase.base = m_current;

    Pipeline<TimeBase>::put(timebase);

    m_start = std::numeric_limits<std::int_fast64_t>::max();
    m_end = std::numeric_limits<std::int_fast64_t>::min();
}
}
