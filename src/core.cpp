#include "core.h"
#include "utility/log.h"

#include "sink/base.h"
#include "source/base.h"
#include "utility/criterion.h"
#include "utility/eventconstructor.h"
#include "messages/event.h"
#include "messages/detectorinfo.h"
#include "messages/clusterlog.h"
#include "supervision/timebase.h"
#include "detectortracker.h"

#include <cinttypes>

namespace MuonPi {

Core::Core(Sink::Base<Event>& event_sink, DetectorTracker& detector_tracker, StateSupervisor& supervisor)
    : Sink::Threaded<Event> { "Core", std::chrono::milliseconds {100} }
    , Source::Base<Event> { event_sink }
    , m_detector_tracker { (detector_tracker) }
    , m_supervisor { supervisor }
{
}

auto Core::supervisor() -> StateSupervisor&
{
    return m_supervisor;
}

auto Core::process() -> int
{
    if (m_supervisor.step() != 0) {
        Log::error()<<"The Supervisor stopped.";
        return -1;
    }

    {
        using namespace std::chrono;
        m_timeout = milliseconds{static_cast<long>(static_cast<double>(duration_cast<milliseconds>(m_time_base_supervisor->current()).count()) * m_detector_tracker.factor())};
        m_supervisor.time_status(duration_cast<milliseconds>(m_timeout));
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

auto Core::post_run() -> int
{
    int result { 0 };

    m_detector_tracker.stop();

    return m_detector_tracker.wait() + result;
}

auto Core::process(Event event) -> int
{

    if (!m_detector_tracker.accept(event)) {
        return 0;
    }

    m_time_base_supervisor->process_event(event);

    m_supervisor.increase_event_count(true);

    std::queue<std::size_t> matches {};
    for (std::size_t i { 0 }; i < m_constructors.size(); i++)  {
        auto& constructor { m_constructors[i] };
        if (m_criterion->maximum_false() < m_criterion->criterion(event, constructor.event)) {
            matches.push(i);
        }
    }

    // +++ Event matches exactly one existing constructor
    if (matches.size() == 1) {
        EventConstructor& constructor { m_constructors[matches.front()] };
        matches.pop();
        if (constructor.event.n() == 1) {
            Event e { std::move(constructor.event) };
            constructor.event = Event{std::move(e), true};
        }
        constructor.event.add_event(std::move(event));
        return 0;
    }
    // --- Event matches exactly one existing constructor

    // +++ Event matches either no, or more than one constructor
    if (matches.empty()) {
        EventConstructor constructor {};
        constructor.event = std::move(event);
        constructor.timeout = m_timeout;
        m_constructors.push_back(std::move(constructor));
        return 0;
    }
    EventConstructor& constructor { m_constructors[matches.front()] };
    matches.pop();
    if (constructor.event.n() == 1) {
        Event e { std::move(constructor.event) };
        constructor.event = Event{std::move(e), true};
    }
    constructor.event.add_event(event);
    // +++ Event matches more than one constructor
    // Combines all contesting constructors into one contesting coincience
    while (!matches.empty()) {
        constructor.event.add_event(std::move(m_constructors[matches.front()].event));
        m_constructors.erase(m_constructors.begin() + static_cast<ssize_t>(matches.front()));
        matches.pop();
    }
    // --- Event matches more than one constructor
    // --- Event matches either no, or more than one constructor
    m_supervisor.set_queue_size(m_constructors.size());
    return 0;
}

}
