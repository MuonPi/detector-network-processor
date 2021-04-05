#include "analysis/coincidencefilter.h"

#include "utility/log.h"

#include "analysis/criterion.h"
#include "messages/clusterlog.h"
#include "messages/detectorinfo.h"
#include "messages/event.h"
#include "sink/base.h"
#include "source/base.h"
#include "supervision/timebase.h"

#include <cinttypes>

namespace muonpi {

constexpr std::chrono::duration s_timeout { std::chrono::milliseconds { 100 } };

coincidence_filter::coincidence_filter(sink::base<event_t>& event_sink, supervision::state& supervisor)
    : sink::threaded<event_t> { "coincidence_filter", s_timeout }
    , source::base<event_t> { event_sink }
    , m_supervisor { supervisor }
{
}

void coincidence_filter::get(timebase_t timebase)
{
    using namespace std::chrono;
    m_timeout = milliseconds { static_cast<long>(static_cast<double>(duration_cast<milliseconds>(timebase.base).count()) * timebase.factor) };
    m_supervisor.time_status(duration_cast<milliseconds>(timebase.base), duration_cast<milliseconds>(m_timeout));
}

void coincidence_filter::get(event_t event)
{
    threaded<event_t>::internal_get(event);
}

auto coincidence_filter::process() -> int
{
    auto now { std::chrono::system_clock::now() };

    // +++ Send finished constructors off to the event sink
    for (ssize_t i { static_cast<ssize_t>(m_constructors.size()) - 1 }; i >= 0; i--) {
        auto& constructor { m_constructors[static_cast<std::size_t>(i)] };
        constructor.set_timeout(m_timeout);
        if (constructor.timed_out(now)) {
            m_supervisor.increase_event_count(false, constructor.event.n());
            put(constructor.event);
            m_constructors.erase(m_constructors.begin() + i);
        }
    }

    m_supervisor.set_queue_size(m_constructors.size());
    return 0;
}

auto coincidence_filter::process(event_t event) -> int
{
    m_supervisor.increase_event_count(true);

    std::queue<std::size_t> matches {};
    for (std::size_t i { 0 }; i < m_constructors.size(); i++) {
        auto& constructor { m_constructors[i] };
        if (constructor.event.hash == event.hash) {
            continue;
        }
        if (m_criterion->maximum_false() < m_criterion->apply(event, constructor.event)) {
            matches.push(i);
        }
    }
    m_supervisor.set_queue_size(m_constructors.size());

    // +++ Event matches exactly one existing constructor
    if (matches.size() == 1) {
        event_constructor& constructor { m_constructors[matches.front()] };
        matches.pop();
        if (constructor.event.n() < 2) {
            event_t e { std::move(constructor.event) };
            event_t new_e { e };
            new_e.end = e.end;
            new_e.emplace(std::move(e));
            constructor.event = std::move(new_e);
        }
        constructor.event.emplace(std::move(event));
        return 0;
    }
    // --- Event matches exactly one existing constructor

    // +++ Event matches either no, or more than one constructor
    if (matches.empty()) {
        event_constructor constructor {};
        constructor.event = event;
        constructor.timeout = m_timeout;
        m_constructors.emplace_back(std::move(constructor));
        return 0;
    }
    event_constructor& constructor { m_constructors[matches.front()] };
    matches.pop();
    if (constructor.event.n() < 2) {
        event_t e { constructor.event };
        event_t new_e { e };
        new_e.end = e.end;
        new_e.emplace(std::move(e));
        constructor.event = std::move(new_e);
    }
    constructor.event.emplace(event);
    // +++ Event matches more than one constructor
    // Combines all contesting constructors into one contesting coincience
    while (!matches.empty()) {
        constructor.event.emplace(m_constructors[matches.front()].event);
        m_constructors.erase(m_constructors.begin() + static_cast<ssize_t>(matches.front()));
        matches.pop();
    }
    // --- Event matches more than one constructor
    // --- Event matches either no, or more than one constructor
    return 0;
}

}
