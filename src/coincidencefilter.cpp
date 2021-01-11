#include "coincidencefilter.h"
#include "utility/log.h"

#include "messages/clusterlog.h"
#include "messages/detectorinfo.h"
#include "messages/event.h"
#include "sink/base.h"
#include "source/base.h"
#include "supervision/timebase.h"
#include "utility/criterion.h"
#include "utility/eventconstructor.h"

#include <cinttypes>

namespace MuonPi {

CoincidenceFilter::CoincidenceFilter(Sink::Base<Event>& event_sink, StateSupervisor& supervisor)
    : Sink::Threaded<Event> { "CoincidenceFilter", std::chrono::milliseconds { 100 } }
    , Source::Base<Event> { event_sink }
    , m_supervisor { supervisor }
{
}

void CoincidenceFilter::get(TimeBase timebase)
{
    using namespace std::chrono;
    m_timeout = milliseconds { static_cast<long>(static_cast<double>(duration_cast<milliseconds>(timebase.base).count()) * timebase.factor) };
    m_supervisor.time_status(duration_cast<milliseconds>(m_timeout));
}

void CoincidenceFilter::get(Event event)
{
    Threaded<Event>::internal_get(event);
}

auto CoincidenceFilter::process() -> int
{
    if (m_supervisor.step() != 0) {
        Log::error() << "The Supervisor stopped.";
        return -1;
    }

    // +++ Send finished constructors off to the event sink
    for (ssize_t i { static_cast<ssize_t>(m_constructors.size()) - 1 }; i >= 0; i--) {
        auto& constructor { m_constructors[static_cast<std::size_t>(i)] };
        constructor.set_timeout(m_timeout);
        if (constructor.timed_out()) {
            m_supervisor.increase_event_count(false, constructor.event.n());
            put(constructor.event);
            m_constructors.erase(m_constructors.begin() + i);
        }
    }

    m_supervisor.set_queue_size(m_constructors.size());
    return 0;
}

auto CoincidenceFilter::process(Event event) -> int
{
    m_supervisor.increase_event_count(true);

    std::queue<std::size_t> matches {};
    for (std::size_t i { 0 }; i < m_constructors.size(); i++) {
        auto& constructor { m_constructors[i] };
        if (m_criterion->maximum_false() < m_criterion->criterion(event, constructor.event)) {
            matches.push(i);
        }
    }
    m_supervisor.set_queue_size(m_constructors.size());

    // +++ Event matches exactly one existing constructor
    if (matches.size() == 1) {
        EventConstructor& constructor { m_constructors[matches.front()] };
        matches.pop();
        if (constructor.event.n() == 1) {
            Event e { constructor.event };
            constructor.event = Event { e, true };
        }
        constructor.event.add_event(event);
        return 0;
    }
    // --- Event matches exactly one existing constructor

    // +++ Event matches either no, or more than one constructor
    if (matches.empty()) {
        EventConstructor constructor {};
        constructor.event = event;
        constructor.timeout = m_timeout;
        m_constructors.push_back(std::move(constructor));
        return 0;
    }
    EventConstructor& constructor { m_constructors[matches.front()] };
    matches.pop();
    if (constructor.event.n() == 1) {
        Event e { constructor.event };
        constructor.event = Event { e, true };
    }
    constructor.event.add_event(event);
    // +++ Event matches more than one constructor
    // Combines all contesting constructors into one contesting coincience
    while (!matches.empty()) {
        constructor.event.add_event(m_constructors[matches.front()].event);
        m_constructors.erase(m_constructors.begin() + static_cast<ssize_t>(matches.front()));
        matches.pop();
    }
    // --- Event matches more than one constructor
    // --- Event matches either no, or more than one constructor
    return 0;
}

}
