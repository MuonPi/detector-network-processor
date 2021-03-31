#include "detectortracker.h"

#include "detector.h"
#include "messages/detectorinfo.h"
#include "messages/detectorsummary.h"
#include "messages/event.h"
#include "source/base.h"
#include "utility/log.h"

#include "defaults.h"

#include "supervision/state.h"

namespace muonpi {

detector_tracker::detector_tracker(sink::base<detetor_summary_t>& summary_sink, sink::base<trigger::detector>& trigger_sink, sink::base<event_t>& event_sink, sink::base<timebase_t>& timebase_sink, state_supervisor& supervisor)
    : sink::threaded<detetor_info_t<location_t>> { "detector_tracker", std::chrono::milliseconds { 100 } }
    , source::base<detetor_summary_t> { summary_sink }
    , source::base<trigger::detector> { trigger_sink }
    , pipeline<event_t> { event_sink }
    , source::base<timebase_t> { timebase_sink }
    , m_supervisor { supervisor }
{
}

void detector_tracker::get(event_t event)
{
    auto det_iterator { m_detectors.find(event.hash()) };
    if (det_iterator == m_detectors.end()) {
        return;
    }
    auto& det { (*det_iterator).second };

    if (!det->process(event)) {
        return;
    }

    event.set_detector_info(det->location(), det->user_info());

    if (det->is(detector::Status::Reliable)) {
        source::base<event_t>::put(event);
    }
}

void detector_tracker::get(detetor_info_t<location_t> detector_info)
{
    threaded<detetor_info_t<location_t>>::internal_get(std::move(detector_info));
}

auto detector_tracker::process(detetor_info_t<location_t> log) -> int
{
    auto det { m_detectors.find(log.hash()) };
    if (det == m_detectors.end()) {
        m_detectors[log.hash()] = std::make_unique<detector>(log, *this);
        m_detectors[log.hash()]->enable();
        save();
        return 0;
    }
    (*det).second->process(log);
    return 0;
}

auto detector_tracker::process() -> int
{
    using namespace std::chrono;

    double largest { 1.0 };
    std::size_t reliable { 0 };
    for (auto& [hash, det] : m_detectors) {

        det->step();

        if (det->is(detector::Status::Reliable)) {
            reliable++;
            if (det->factor() > largest) {
                largest = det->factor();
            }
        }
    }

    source::base<timebase_t>::put(timebase_t { largest });

    while (!m_delete_detectors.empty()) {
        m_detectors.erase(m_delete_detectors.front());
        m_delete_detectors.pop();
    }

    // +++ push detector log messages at regular interval
    steady_clock::time_point now { steady_clock::now() };

    if ((now - m_last) >= Config::interval.detectorsummary) {
        m_last = now;

        for (auto& [hash, det] : m_detectors) {
            source::base<detetor_summary_t>::put(det->current_log_data());
        }
    }
    // --- push detector log messages at regular interval

    return 0;
}

void detector_tracker::get(trigger::detector::action_t action)
{
    std::size_t hash { std::hash<std::string> {}(action.setting.username + action.setting.station) };
    if (action.type == trigger::detector::action_t::Activate) {
        if (m_detector_triggers.find(hash) == m_detector_triggers.end()) {
            m_detector_triggers[hash] = {};
        }
        m_detector_triggers[hash][action.setting.type] = action.setting;
    } else {
        m_detector_triggers[hash].erase(action.setting.type);
        if (m_detector_triggers[hash].empty()) {
            m_detector_triggers.erase(hash);
        }
    }
}

void detector_tracker::detector_status(std::size_t hash, detector::Status status)
{
    auto user_info { m_detectors[hash]->user_info() };
    if (status > detector::Status::Deleted) {
        source::base<detetor_summary_t>::put(m_detectors[hash]->change_log_data());
    }
    m_supervisor.detector_status(hash, status);

    switch (status) {
    case detector::Status::Deleted:
        m_delete_detectors.push(hash);
        if (m_detector_triggers.find(hash) == m_detector_triggers.end()) {
            return;
        }
        if (m_detector_triggers[hash].find(trigger::detector::setting_t::Type::Offline) == m_detector_triggers[hash].end()) {
            return;
        }
        source::base<trigger::detector>::put({ m_detector_triggers[hash][trigger::detector::setting_t::Type::Offline] });
        return;
    case detector::Status::Created:
        if (m_detector_triggers.find(hash) == m_detector_triggers.end()) {
            return;
        }
        if (m_detector_triggers[hash].find(trigger::detector::setting_t::Type::Online) == m_detector_triggers[hash].end()) {
            return;
        }
        source::base<trigger::detector>::put({ m_detector_triggers[hash][trigger::detector::setting_t::Type::Online] });
        break;
    case detector::Status::Reliable:
        if (m_detector_triggers.find(hash) == m_detector_triggers.end()) {
            return;
        }
        if (m_detector_triggers[hash].find(trigger::detector::setting_t::Type::Reliable) == m_detector_triggers[hash].end()) {
            return;
        }
        source::base<trigger::detector>::put({ m_detector_triggers[hash][trigger::detector::setting_t::Type::Reliable] });
        break;
    case detector::Status::Unreliable:
        if (m_detector_triggers.find(hash) == m_detector_triggers.end()) {
            return;
        }
        if (m_detector_triggers[hash].find(trigger::detector::setting_t::Type::Unreliable) == m_detector_triggers[hash].end()) {
            return;
        }
        source::base<trigger::detector>::put({ m_detector_triggers[hash][trigger::detector::setting_t::Type::Unreliable] });
        break;
    }
    save();
}

void detector_tracker::load()
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
            auto det { std::make_unique<detector>(line, *this, stale) };
            if (det->is(detector::Status::Deleted)) {
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

void detector_tracker::save()
{
    std::ofstream out { Config::files.state };

    if (!out.is_open()) {
        log::warning() << "Could not save detectors.";
        return;
    }
    out << std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count() << '\n';
    for (auto& [hash, det] : m_detectors) {
        if (det->is(detector::Status::Deleted)) {
            continue;
        }
        out << det->serialise() << '\n';
    }
    out.close();
}
}
