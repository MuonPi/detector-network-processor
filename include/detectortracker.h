#ifndef DETECTORTRACKER_H
#define DETECTORTRACKER_H

#include "sink/base.h"
#include "source/base.h"
#include "detector.h"

#include "messages/trigger.h"

#include <map>
#include <memory>
#include <queue>


namespace MuonPi {

class Event;
class DetectorInfo;
class DetectorSummary;
class StateSupervisor;


class DetectorTracker : public Sink::Threaded<DetectorInfo>, public Source::Base<DetectorSummary>, public Source::Base<DetectorTrigger>
{
public:
    /**
     * @brief DetectorTracker
     * @param summary_sink A Sink to write the detector summaries to.
     * @param trigger_sink A Sink to write the detector triggers to.
     * @param supervisor A reference to a supervisor object, which keeps track of program metadata
     */
    DetectorTracker(Sink::Base<DetectorSummary>& summary_sink, Sink::Base<DetectorTrigger>& trigger_sink, StateSupervisor& supervisor);

    /**
     * @brief accept Check if an event is accepted
     * @param event The event to check
     * @return true if the event belongs to a known detector and the detector is reliable
     */
    [[nodiscard]] auto accept(Event &event) -> bool;

    /**
     * @brief factor The current maximum factor
     * @return maximum factor between all detectors
     */
    [[nodiscard]] auto factor() const -> double;

    /**
     * @brief detector_status Update the status of one detector
     * @param hash The hashed detector identifier
     * @param status The new status of the detector
     */
    void detector_status(std::size_t hash, Detector::Status status);

protected:

    /**
     * @brief process Process a log message. Hands the message over to a detector, if none exists, creates a new one.
     * @param log The log message to check
     */
    [[nodiscard]] auto process(DetectorInfo log) -> int override;
    [[nodiscard]] auto process() -> int override;

private:
    StateSupervisor& m_supervisor;

    std::map<std::size_t, std::unique_ptr<Detector>> m_detectors {};

    std::queue<std::size_t> m_delete_detectors {};

    double m_factor { 1.0 };

    std::chrono::steady_clock::time_point m_last { std::chrono::steady_clock::now() };
};

}

#endif // DETECTORTRACKER_H
