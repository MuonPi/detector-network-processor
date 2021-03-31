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

namespace muonpi {

/**
 * @brief The coincidence_filter class
 */
class coincidence_filter : public sink::threaded<Event>, public source::base<Event>, public sink::base<Timebase> {
public:
    /**
     * @brief coincidence_filter
     * @param event_sink A collection of event sinks to use
     * @param supervisor A reference to a StateSupervisor, which keeps track of program metadata
     */
    coincidence_filter(sink::base<Event>& event_sink, StateSupervisor& supervisor);

    ~coincidence_filter() override = default;

    void get(Timebase timebase) override;
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
