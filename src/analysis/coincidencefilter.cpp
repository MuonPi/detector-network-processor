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
    : sink::threaded<event_t> { "muon::filter", s_timeout }
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
        bool skip { false };
        auto check_e_hash { [](const event_t::data_t& data, const event_t& e) {
            if (e.n() < 2) {
                return data.hash == e.data.hash;
            }
            return std::any_of(e.events.begin(), e.events.end(), [&](const event_t::data_t& d) { return d.hash == data.hash; });
        } };
        if (constructor.event.n() > 1) {
            skip = std::any_of(constructor.event.events.begin(), constructor.event.events.end(), [&](const event_t::data_t& d) { return check_e_hash(d, event); });
        } else if (event.n() > 1) {
            skip = std::any_of(event.events.begin(), event.events.end(), [&](const event_t::data_t& d) { return check_e_hash(d, constructor.event); });
        } else {
            if (constructor.event.data.hash == event.data.hash) {
                skip = true;
            }
        }
        if (skip) {
            continue;
        }
        const auto result { m_criterion->apply(event, constructor.event) };
        if (result == criterion::Type::Conflicting) {
            matches.push(i);
            if (constructor.event.n() > 1) {
                constructor.event.conflicting = true;
            }
        } else if (result == criterion::Type::Valid) {
            matches.push(i);
        }
    }
    m_supervisor.set_queue_size(m_constructors.size());

    // +++ Event matches exactly one existing constructor
    if (matches.size() == 1) {
        event_constructor& constructor { m_constructors[matches.front()] };
        matches.pop();
        if (constructor.event.n() < 2) {
            event_t e { constructor.event };
            constructor.event.data.end = constructor.event.data.start;
            constructor.event.emplace(e);
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
        constructor.event.data.end = constructor.event.data.start;
        constructor.event.emplace(e);
    }
    constructor.event.emplace(event);
    constructor.event.conflicting = true;
    // Combines all contesting constructors into one contesting coincience
    while (!matches.empty()) {
        constructor.event.emplace(m_constructors[matches.front()].event);
        m_constructors.erase(m_constructors.begin() + static_cast<ssize_t>(matches.front()));
        matches.pop();
    }
    // --- Event matches either no, or more than one constructor
    return 0;
}

} // namespace muonpi
