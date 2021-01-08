#include "supervision/timebase.h"

#include "messages/event.h"

#include "utility/log.h"

namespace MuonPi {

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
    if ((std::chrono::system_clock::now() - m_sample_start) < m_sample_time) {
        timebase.base = m_current;
        Pipeline<TimeBase>::put(timebase);
        return;
    }

    m_current = std::chrono::nanoseconds { m_end - m_start };

    m_start += 10000000000000;
    m_end = -1000000000;
    m_sample_start = std::chrono::system_clock::now();
    if (m_current < s_minimum) {
        m_current = s_minimum;
    } else if (m_current > s_maximum) {
        m_current = s_maximum;
    }
    timebase.base = m_current;
    Pipeline<TimeBase>::put(timebase);
}
}
