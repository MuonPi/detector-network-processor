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
    : sink::threaded<detector_info_t<location_t>> { "detector_tracker", s_timeout }
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

    if (det->is(detector_station::Status::Reliable)) {
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
        m_detectors[log.hash] = std::make_unique<detector_station>(log, *this);
        m_detectors[log.hash]->enable();
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

            if (det->is(detector_station::Status::Reliable)) {
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

    if ((now - m_last) >= Config::interval.detectorsummary) {
        m_last = now;

        for (auto& [hash, det] : m_detectors) {
            source::base<detector_summary_t>::put(det->current_log_data());
        }
    }
    // --- push detector log messages at regular interval

    return 0;
}

void station::detector_status(std::size_t hash, detector_station::Status status)
{
    auto user_info { m_detectors[hash]->user_info() };
    if (status > detector_station::Status::Deleted) {
        source::base<detector_summary_t>::put(m_detectors[hash]->change_log_data());
    }
    m_supervisor.detector_status(hash, status);

    trigger::detector trigger {};

    trigger.hash = hash;
    trigger.setting.username = user_info.username;
    trigger.setting.station = user_info.station_id;

    switch (status) {
    case detector_station::Status::Deleted:
        m_delete_detectors.push(hash);
        trigger.setting.type = trigger::detector::setting_t::Type::Offline;
        break;
    case detector_station::Status::Created:
        trigger.setting.type = trigger::detector::setting_t::Type::Online;
        break;
    case detector_station::Status::Reliable:
        trigger.setting.type = trigger::detector::setting_t::Type::Reliable;
        break;
    case detector_station::Status::Unreliable:
        trigger.setting.type = trigger::detector::setting_t::Type::Unreliable;
        break;
    }
    source::base<trigger::detector>::put(std::move(trigger));
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

void station::load()
{
    std::ifstream in { Config::files.state };

    if (!in.is_open()) {
        log::warning() << "Could not load detectors.";
        return;
    }
    try {
        log::info() << "Loading detector states.";
        std::string line {};
        std::getline(in, line);
        std::chrono::system_clock::time_point state { std::chrono::seconds { std::stoll(line, nullptr) } };
        bool stale { false };
        if ((state + std::chrono::minutes { 3 }) > std::chrono::system_clock::now()) {
            log::warning() << "detector state stale, marking all detectors unreliable.";
            stale = true;
        }
        for (; std::getline(in, line);) {
            auto det { std::make_unique<detector_station>(line, *this, stale) };
            if (det->is(detector_station::Status::Deleted)) {
                continue;
            }
            m_detectors.emplace(det->user_info().hash(), std::move(det));
        }
        in.close();
    } catch (...) {
        log::warning() << "Could not load detectors.";
        return;
    }
}

void station::save()
{
    std::ofstream out { Config::files.state };

    if (!out.is_open()) {
        log::warning() << "Could not save detectors.";
        return;
    }
    out << std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count() << '\n';
    for (auto& [hash, det] : m_detectors) {
        if (det->is(detector_station::Status::Deleted)) {
            continue;
        }
        out << det->serialise() << '\n';
    }
    out.close();
}
}
