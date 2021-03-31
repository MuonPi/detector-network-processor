#include "supervision/timebase.h"

#include "messages/event.h"

#include "utility/log.h"

#include <algorithm>

namespace muonpi {

timebase_supervisor::timebase_supervisor(sink::base<event_t>& event_sink, sink::base<timebase_t>& timebase_sink)
    : pipeline<event_t> { event_sink }
    , pipeline<timebase_t> { timebase_sink }
{
}

void timebase_supervisor::get(event_t event)
{
    if (event.start() < m_start) {
        m_start = event.start();
    } else if (event.start() > m_end) {
        m_end = event.start();
    }
    pipeline<event_t>::put(event);
}

void timebase_supervisor::get(timebase_t timebase)
{
    if ((std::chrono::system_clock::now() - m_sample_start) < s_sample_time) {
        timebase.base = m_current;
        pipeline<timebase_t>::put(timebase);
        return;
    }

    m_sample_start = std::chrono::system_clock::now();

    m_current = std::clamp(std::chrono::nanoseconds { m_end - m_start }, s_minimum, s_maximum);

    timebase.base = m_current;

    pipeline<timebase_t>::put(timebase);

    m_start = std::numeric_limits<std::int_fast64_t>::max();
    m_end = std::numeric_limits<std::int_fast64_t>::min();
}
}
