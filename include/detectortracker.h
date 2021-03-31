#ifndef DETECTORTRACKER_H
#define DETECTORTRACKER_H

#include "pipeline.h"
#include "sink/base.h"
#include "source/base.h"

#include "detector.h"

#include "messages/detectorinfo.h"
#include "messages/event.h"
#include "messages/trigger.h"

#include <map>
#include <memory>
#include <queue>

namespace MuonPi {

class DetectorSummary;
class StateSupervisor;

class detector_tracker
    : public Sink::Threaded<DetectorInfo<Location>>,
      public Source::Base<DetectorSummary>,
      public Source::Base<Trigger::Detector>,
      public Sink::Base<Trigger::Detector::Action>,
      public Pipeline<Event>,
      public Source::Base<TimeBase> {
public:
    /**
     * @brief detector_tracker
     * @param summary_sink A Sink to write the detector summaries to.
     * @param trigger_sink A Sink to write the detector triggers to.
     * @param event_sink A Sink to write the events to.
     * @param supervisor A reference to a supervisor object, which keeps track of program metadata
     */
    detector_tracker(Sink::Base<DetectorSummary>& summary_sink, Sink::Base<Trigger::Detector>& trigger_sink, Sink::Base<Event>& event_sink, Sink::Base<TimeBase>& timebase_sink, StateSupervisor& supervisor);

    /**
     * @brief detector_status Update the status of one detector
     * @param hash The hashed detector identifier
     * @param status The new status of the detector
     */
    void detector_status(std::size_t hash, Detector::Status status);

    void get(Trigger::Detector::Action action) override;

    void get(Event event) override;

    void get(DetectorInfo<Location> detector_info) override;

protected:
    /**
     * @brief process Process a log message. Hands the message over to a detector, if none exists, creates a new one.
     * @param log The log message to check
     */
    [[nodiscard]] auto process(DetectorInfo<Location> log) -> int override;
    [[nodiscard]] auto process() -> int override;

    void save();
    void load();

private:
    StateSupervisor& m_supervisor;

    std::map<std::size_t, std::unique_ptr<Detector>> m_detectors {};

    std::queue<std::size_t> m_delete_detectors {};

    std::chrono::steady_clock::time_point m_last { std::chrono::steady_clock::now() };

    std::map<std::size_t, std::map<Trigger::Detector::Setting::Type, Trigger::Detector::Setting>> m_detector_triggers {};
};

}

#endif // DETECTORTRACKER_H
