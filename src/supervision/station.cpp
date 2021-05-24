#include "supervision/station.h"

#include "messages/detectorinfo.h"
#include "messages/detectorsummary.h"
#include "messages/event.h"
#include "source/base.h"
#include "utility/log.h"

#include "defaults.h"

#include "supervision/state.h"

namespace muonpi::supervision {

constexpr static std::chrono::duration s_timeout { std::chrono::milliseconds { 100 } };

station::station(sink::base<detector_summary_t>& summary_sink, sink::base<trigger::detector>& trigger_sink, sink::base<event_t>& event_sink, sink::base<timebase_t>& timebase_sink, supervision::state& supervisor)
    : sink::threaded<detector_info_t<location_t>> { "muon::station", s_timeout }
    , source::base<detector_summary_t> { summary_sink }
    , source::base<trigger::detector> { trigger_sink }
    , pipeline::base<event_t> { event_sink }
    , source::base<timebase_t> { timebase_sink }
    , m_supervisor { supervisor }
{
}

void station::get(event_t event)
{
    auto det_iterator { m_detectors.find(event.data.hash) };
    if (det_iterator == m_detectors.end()) {
        return;
    }
    auto& det { (*det_iterator).second };

    if (!det->process(event)) {
        return;
    }

    event.data.location = det->location();
    event.data.userinfo = det->user_info();

    if (det->is(detector_status::reliable)) {
        source::base<event_t>::put(std::move(event));
    }
}

void station::get(detector_info_t<location_t> detector_info)
{
    threaded<detector_info_t<location_t>>::internal_get(std::move(detector_info));
}

auto station::process(detector_info_t<location_t> log) -> int
{
    auto det { m_detectors.find(log.hash) };
    if (det == m_detectors.end()) {
        m_detectors.emplace(log.hash, std::make_unique<detector_station>(log, *this));
        m_detectors.at(log.hash)->enable();
        return 0;
    }
    (*det).second->process(log);
    return 0;
}

auto station::process() -> int
{
    using namespace std::chrono;
    {
        double largest { 1.0 };
        system_clock::time_point now { system_clock::now() };
        for (auto& [hash, det] : m_detectors) {

            det->step(now);

            if (det->is(detector_status::reliable)) {
                if (det->factor() > largest) {
                    largest = det->factor();
                }
            }
        }
        source::base<timebase_t>::put(timebase_t { largest });
    }

    while (!m_delete_detectors.empty()) {
        m_detectors.erase(m_delete_detectors.front());
        m_delete_detectors.pop();
    }

    // +++ push detector log messages at regular interval
    steady_clock::time_point now { steady_clock::now() };

    if ((now - m_last) >= config::singleton()->interval.detectorsummary) {
        m_last = now;

        for (auto& [hash, det] : m_detectors) {
            source::base<detector_summary_t>::put(det->current_log_data());
        }
    }
    // --- push detector log messages at regular interval

    return 0;
}

void station::on_detector_status(std::size_t hash, detector_status::status status, detector_status::reason reason)
{
    if (status > detector_status::deleted) {
        source::base<detector_summary_t>::put(m_detectors.at(hash)->change_log_data());
    }
    m_supervisor.on_detector_status(hash, status);

    if (status == detector_status::deleted) {
        m_delete_detectors.push(hash);
    }
    source::base<trigger::detector>::put(trigger::detector { hash, m_detectors.at(hash)->user_info(), status, reason });
}

auto station::get_stations() const -> std::vector<std::pair<userinfo_t, location_t>>
{
    std::vector<std::pair<userinfo_t, location_t>> stations {};
    for (const auto& [hash, stat] : m_detectors) {
        stations.emplace_back(std::make_pair(stat->user_info(), stat->location()));
    }
    return stations;
}

auto station::get_station(std::size_t hash) const -> std::pair<userinfo_t, location_t>
{
    const auto it { m_detectors.find(hash) };
    if (it == m_detectors.end()) {
        return {};
    }
    return std::make_pair(it->second->user_info(), it->second->location());
}

} // namespace muonpi::supervision
