#include "analysis/coincidencefilter.h"

#include "analysis/criterion.h"
#include "messages/clusterlog.h"
#include "messages/detectorinfo.h"
#include "messages/event.h"
#include "supervision/timebase.h"

#include <muonpi/log.h>
#include <muonpi/scopeguard.h>
#include <muonpi/sink/base.h>
#include <muonpi/source/base.h>

#include <cinttypes>
#include <stack>

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
    m_timeout = timebase.timeout();
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
    for (auto it { m_constructors.begin() }; it != m_constructors.end();) {
        event_constructor& constructor { *it };
        constructor.set_timeout(m_timeout);
        if (constructor.timed_out(now)) {
            m_supervisor.process_event(constructor.event, false);
            put(constructor.event);
            it = m_constructors.erase(it);
        } else {
            ++it;
        }
    }

    m_supervisor.set_queue_size(m_constructors.size());
    return 0;
}

auto coincidence_filter::next_match(const event_t& event, std::list<event_constructor>::iterator start) -> std::pair<criterion::score_t, std::list<event_constructor>::iterator>
{
    if (start == m_constructors.end()) {
        return std::make_pair(criterion::score_t {}, start);
    }

    for (auto it { start }; it != m_constructors.end(); ++it) {
        event_constructor& constructor { *it };
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
        } else if (constructor.event.data.hash == event.data.hash) {
            skip = true;
        }
        if (skip) {
            continue;
        }
        const auto result { m_criterion->apply(event, constructor.event) };
        if (result) {
            return std::make_pair(result, it);
        }
    }
    return std::make_pair(criterion::score_t {}, m_constructors.end());
}

auto coincidence_filter::process(event_t event) -> int
{
    m_supervisor.process_event(event, true);
    const scope_guard guard { [&]() {
        m_supervisor.set_queue_size(m_constructors.size());
    } };

    auto [score, iterator] { next_match(event, m_constructors.begin()) };

    if (iterator == m_constructors.end()) {
        event_constructor constructor {};
        constructor.event = event;
        constructor.timeout = m_timeout;
        m_constructors.emplace_back(std::move(constructor));
        return 0;
    }

    event_constructor& constructor { *iterator };

    if (constructor.event.n() < 2) {
        event_t e { constructor.event };
        constructor.event.data.end = constructor.event.data.start;
        constructor.event.emplace(e);
    }
    if (!score) {
        constructor.event.conflicting = true;
    }
    constructor.event.true_e += score.true_e;
    constructor.event.emplace(std::move(event));

    do {
        ++iterator;
        auto match = next_match(event, iterator);
        score = match.first;
        iterator = match.second;
        if (iterator == m_constructors.end()) {
            return 0;
        }

        constructor.event.conflicting = true;
        constructor.event.true_e += score.true_e;

        constructor.event.emplace((*iterator).event);

        iterator = m_constructors.erase(iterator);

    } while (iterator != m_constructors.end());

    return 0;
}

} // namespace muonpi
