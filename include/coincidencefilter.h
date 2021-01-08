#ifndef COINCIDENCEFILTER_H
#define COINCIDENCEFILTER_H

#include "detector.h"
#include "messages/clusterlog.h"
#include "supervision/state.h"
#include "supervision/timebase.h"
#include "utility/coincidence.h"
#include "utility/eventconstructor.h"
#include "utility/threadrunner.h"

#include <pipeline.h>

#include <map>
#include <queue>
#include <vector>

namespace MuonPi {

/**
 * @brief The CoincidenceFilter class
 */
class CoincidenceFilter : public Sink::Threaded<Event>, public Source::Base<Event>, public Sink::Base<TimeBase> {
public:
    /**
     * @brief CoincidenceFilter
     * @param event_sink A collection of event sinks to use
     * @param supervisor A reference to a StateSupervisor, which keeps track of program metadata
     */
    CoincidenceFilter(Sink::Base<Event>& event_sink, StateSupervisor& supervisor);

    ~CoincidenceFilter() override = default;

    void get(TimeBase timebase) override;
    void get(Event event) override;

protected:
    /**
     * @brief process Called from step(). Handles a new event arriving
     * @param event The event to process
     */
    [[nodiscard]] auto process(Event event) -> int override;
    [[nodiscard]] auto process() -> int override;

private:
    std::unique_ptr<Criterion> m_criterion { std::make_unique<Coincidence>() };

    std::vector<EventConstructor> m_constructors {};

    std::chrono::system_clock::duration m_timeout { std::chrono::seconds { 10 } };

    StateSupervisor& m_supervisor;
};

}

#endif // COINCIDENCEFILTER_H
