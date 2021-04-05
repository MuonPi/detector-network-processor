#ifndef COINCIDENCEFILTER_H
#define COINCIDENCEFILTER_H

#include "analysis/coincidence.h"
#include "analysis/detectorstation.h"
#include "analysis/eventconstructor.h"
#include "messages/clusterlog.h"
#include "supervision/state.h"
#include "supervision/timebase.h"
#include "utility/threadrunner.h"

#include <map>
#include <queue>
#include <vector>

namespace muonpi {

/**
 * @brief The coincidence_filter class
 */
class coincidence_filter : public sink::threaded<event_t>, public source::base<event_t>, public sink::base<timebase_t> {
public:
    /**
     * @brief coincidence_filter
     * @param event_sink A collection of event sinks to use
     * @param supervisor A reference to a state_supervisor, which keeps track of program metadata
     */
    coincidence_filter(sink::base<event_t>& event_sink, supervision::state& supervisor);

    ~coincidence_filter() override = default;

    void get(timebase_t timebase) override;
    void get(event_t event) override;

protected:
    /**
     * @brief process Called from step(). Handles a new event arriving
     * @param event The event to process
     */
    [[nodiscard]] auto process(event_t event) -> int override;
    [[nodiscard]] auto process() -> int override;

private:
    std::unique_ptr<criterion> m_criterion { std::make_unique<coincidence>() };

    std::vector<event_constructor> m_constructors {};

    std::chrono::system_clock::duration m_timeout { std::chrono::seconds { 10 } };

    supervision::state& m_supervisor;
};

}

#endif // COINCIDENCEFILTER_H
