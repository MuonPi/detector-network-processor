#ifndef CORE_H
#define CORE_H

#include "utility/threadrunner.h"
#include "detector.h"
#include "utility/coincidence.h"
#include "supervision/timebase.h"
#include "utility/eventconstructor.h"
#include "supervision/state.h"
#include "messages/clusterlog.h"
#include "sink/base.h"
#include "source/base.h"

#include <queue>
#include <map>
#include <vector>


namespace MuonPi {

class DetectorTracker;

/**
 * @brief The Core class
 */
class Core : public Sink::Threaded<Event>, public Source::Base<Event>
{
public:
    /**
     * @brief Core
     * @param event_sink A collection of event sinks to use
     * @param detector_tracker A reference to the detector tracker which keeps track of connected detectors
     * @param supervisor A reference to a StateSupervisor, which keeps track of program metadata
     */
    Core(Sink::Base<Event>& event_sink, DetectorTracker& detector_tracker, StateSupervisor& supervisor);

    /**
     * @brief supervisor Acceess the supervision object
     */
    [[nodiscard]] auto supervisor() -> StateSupervisor&;

    ~Core() override = default;

protected:

    /**
     * @brief post_run reimplemented from ThreadRunner
     * @return zero in case of success.
     */
    [[nodiscard]] auto post_run() -> int override;


    /**
     * @brief process Called from step(). Handles a new event arriving
     * @param event The event to process
     */
    [[nodiscard]] auto process(Event event) -> int override;
    [[nodiscard]] auto process() -> int override;

private:

    DetectorTracker& m_detector_tracker;
    std::unique_ptr<TimeBaseSupervisor> m_time_base_supervisor { std::make_unique<TimeBaseSupervisor>( std::chrono::seconds{2} ) };

    std::unique_ptr<Criterion> m_criterion { std::make_unique<Coincidence>() };

    std::vector<EventConstructor> m_constructors {};

    std::chrono::system_clock::duration m_timeout { std::chrono::seconds{10} };

    StateSupervisor& m_supervisor;


};

}

#endif // CORE_H
