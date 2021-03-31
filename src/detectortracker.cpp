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

detector_tracker::detector_tracker(sink::base<DetectorSummary>& summary_sink, sink::base<Trigger::Detector>& trigger_sink, sink::base<Event>& event_sink, sink::base<Timebase>& timebase_sink, StateSupervisor& supervisor)
    : sink::threaded<DetectorInfo<Location>> { "detector_tracker", std::chrono::milliseconds { 100 } }
    , source::base<DetectorSummary> { summary_sink }
    , source::base<Trigger::Detector> { trigger_sink }
    , pipeline<Event> { event_sink }
    , source::base<Timebase> { timebase_sink }
    , m_supervisor { supervisor }
{
}

void detector_tracker::get(Event event)
{
    auto detector { m_detectors.find(event.hash()) };
    if (detector == m_detectors.end()) {
        return;
    }
    auto& det { (*detector).second };

    if (!det->process(event)) {
        return;
    }

    event.set_detector_info(det->location(), det->user_info());

    if (det->is(Detector::Status::Reliable)) {
        source::base<Event>::put(event);
    }
}

void detector_tracker::get(DetectorInfo<Location> detector_info)
{
    threaded<DetectorInfo<Location>>::internal_get(std::move(detector_info));
}

auto detector_tracker::process(DetectorInfo<Location> log) -> int
{
    auto detector { m_detectors.find(log.hash()) };
    if (detector == m_detectors.end()) {
        m_detectors[log.hash()] = std::make_unique<Detector>(log, *this);
        m_detectors[log.hash()]->enable();
        save();
        return 0;
    }
    (*detector).second->process(log);
    return 0;
}

auto detector_tracker::process() -> int
{
    using namespace std::chrono;

    double largest { 1.0 };
    std::size_t reliable { 0 };
    for (auto& [hash, detector] : m_detectors) {

        detector->step();

        if (detector->is(Detector::Status::Reliable)) {
            reliable++;
            if (detector->factor() > largest) {
                largest = detector->factor();
            }
        }
    }

    source::base<Timebase>::put(Timebase { largest });

    while (!m_delete_detectors.empty()) {
        m_detectors.erase(m_delete_detectors.front());
        m_delete_detectors.pop();
    }

    // +++ push detector log messages at regular interval
    steady_clock::time_point now { steady_clock::now() };

    if ((now - m_last) >= Config::interval.detectorsummary) {
        m_last = now;

        for (auto& [hash, detector] : m_detectors) {
            source::base<DetectorSummary>::put(detector->current_log_data());
        }
    }
    // --- push detector log messages at regular interval

    return 0;
}

void detector_tracker::get(Trigger::Detector::Action action)
{
    std::size_t hash { std::hash<std::string> {}(action.setting.username + action.setting.station) };
    if (action.type == Trigger::Detector::Action::Activate) {
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

void detector_tracker::detector_status(std::size_t hash, Detector::Status status)
{
    auto user_info { m_detectors[hash]->user_info() };
    if (status > Detector::Status::Deleted) {
        source::base<DetectorSummary>::put(m_detectors[hash]->change_log_data());
    }
    m_supervisor.detector_status(hash, status);

    switch (status) {
    case Detector::Status::Deleted:
        m_delete_detectors.push(hash);
        if (m_detector_triggers.find(hash) == m_detector_triggers.end()) {
            return;
        }
        if (m_detector_triggers[hash].find(Trigger::Detector::Setting::Type::Offline) == m_detector_triggers[hash].end()) {
            return;
        }
        source::base<Trigger::Detector>::put({ m_detector_triggers[hash][Trigger::Detector::Setting::Type::Offline] });
        return;
    case Detector::Status::Created:
        if (m_detector_triggers.find(hash) == m_detector_triggers.end()) {
            return;
        }
        if (m_detector_triggers[hash].find(Trigger::Detector::Setting::Type::Online) == m_detector_triggers[hash].end()) {
            return;
        }
        source::base<Trigger::Detector>::put({ m_detector_triggers[hash][Trigger::Detector::Setting::Type::Online] });
        break;
    case Detector::Status::Reliable:
        if (m_detector_triggers.find(hash) == m_detector_triggers.end()) {
            return;
        }
        if (m_detector_triggers[hash].find(Trigger::Detector::Setting::Type::Reliable) == m_detector_triggers[hash].end()) {
            return;
        }
        source::base<Trigger::Detector>::put({ m_detector_triggers[hash][Trigger::Detector::Setting::Type::Reliable] });
        break;
    case Detector::Status::Unreliable:
        if (m_detector_triggers.find(hash) == m_detector_triggers.end()) {
            return;
        }
        if (m_detector_triggers[hash].find(Trigger::Detector::Setting::Type::Unreliable) == m_detector_triggers[hash].end()) {
            return;
        }
        source::base<Trigger::Detector>::put({ m_detector_triggers[hash][Trigger::Detector::Setting::Type::Unreliable] });
        break;
    }
    save();
}

void detector_tracker::load()
{
    std::ifstream in { Config::files.state };

    if (!in.is_open()) {
        Log::warning() << "Could not load detectors.";
        return;
    }
    try {
        Log::info() << "Loading detector states.";
        std::string line {};
        std::getline(in, line);
        std::chrono::system_clock::time_point state { std::chrono::seconds { std::stoll(line, nullptr) } };
        bool stale { false };
        if ((state + std::chrono::minutes { 3 }) > std::chrono::system_clock::now()) {
            Log::warning() << "Detector state stale, marking all detectors unreliable.";
            stale = true;
        }
        for (; std::getline(in, line);) {
            auto detector { std::make_unique<Detector>(line, *this, stale) };
            if (detector->is(Detector::Status::Deleted)) {
                continue;
            }
            m_detectors.emplace(detector->user_info().hash(), std::move(detector));
        }
        in.close();
    } catch (...) {
        Log::warning() << "Could not load detectors.";
        return;
    }
}

void detector_tracker::save()
{
    std::ofstream out { Config::files.state };

    if (!out.is_open()) {
        Log::warning() << "Could not save detectors.";
        return;
    }
    out << std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count() << '\n';
    for (auto& [hash, detector] : m_detectors) {
        if (detector->is(Detector::Status::Deleted)) {
            continue;
        }
        out << detector->serialise() << '\n';
    }
    out.close();
}
}
