#include "detectortracker.h"

#include "messages/event.h"
#include "messages/detectorinfo.h"
#include "messages/detectorsummary.h"
#include "source/base.h"
#include "detector.h"
#include "utility/log.h"

#include "defaults.h"

#include "supervision/state.h"

namespace MuonPi {

DetectorTracker::DetectorTracker(Sink::Base<DetectorSummary>& summary_sink, StateSupervisor& supervisor)
    : Sink::Threaded<DetectorInfo> { "DetectorTracker", std::chrono::milliseconds{100} }
    , m_supervisor { supervisor }
    , m_summary_sink { summary_sink }
{
}

auto DetectorTracker::accept(Event& event) -> bool
{
    auto detector { m_detectors.find(event.hash()) };
    if (detector != m_detectors.end()) {
        auto& det { (*detector).second };
       det->process(event);

        event.set_detector_info(det->location(), det->time_info(), det->user_info());

        return det->is(Detector::Status::Reliable);
    }
    return false;
}

auto DetectorTracker::process(DetectorInfo log) -> int
{
    auto detector { m_detectors.find(log.hash()) };
    if (detector == m_detectors.end()) {
        m_detectors[log.hash()] = std::make_unique<Detector>(log, *this);
        return 0;
    }
    (*detector).second->process(log);
    return 0;
}

auto DetectorTracker::factor() const -> double
{
    return m_factor;
}

auto DetectorTracker::process() -> int
{
    using namespace std::chrono;

    double largest { 1.0 };
    std::size_t reliable { 0 };
    for (auto& [hash, detector]: m_detectors) {

        detector->step();

        if (detector->is(Detector::Status::Reliable)) {
            reliable++;
            if (detector->factor() > largest) {
                largest = detector->factor();
            }
        }
    }

    m_factor = largest;

    while (!m_delete_detectors.empty()) {
        m_detectors.erase(m_delete_detectors.front());
        m_delete_detectors.pop();
    }


    // +++ push detector log messages at regular interval
    steady_clock::time_point now { steady_clock::now() };


    if ((now - m_last) >= Config::Interval::detectorsummary_interval) {
        m_last = now;

        for (auto& [hash, detector]: m_detectors) {
            m_summary_sink.get( detector->current_log_data() );
        }
    }
    // --- push detector log messages at regular interval

    return 0;
}

void DetectorTracker::detector_status(std::size_t hash, Detector::Status status)
{
    if (status == Detector::Status::Deleted) {
        m_delete_detectors.push(hash);
    } else if (status > Detector::Status::Deleted) {
        m_summary_sink.get( m_detectors[hash]->change_log_data() );
    }
    m_supervisor.detector_status(hash, status);
}
}
