#ifndef STATIONSUPERVISION_H
#define STATIONSUPERVISION_H

#include "pipeline/base.h"
#include "sink/base.h"
#include "source/base.h"

#include "analysis/detectorstation.h"

#include "messages/detectorinfo.h"
#include "messages/event.h"
#include "messages/trigger.h"

#include <map>
#include <memory>
#include <queue>

namespace muonpi {

class detetor_summary_t;


namespace supervision {
class state;

class station
    : public sink::threaded<detetor_info_t<location_t>>,
      public source::base<detetor_summary_t>,
      public source::base<trigger::detector>,
      public sink::base<trigger::detector::action_t>,
      public pipeline::base<event_t>,
      public source::base<timebase_t> {
public:
    /**
     * @brief detector_tracker
     * @param summary_sink A sink to write the detector summaries to.
     * @param trigger_sink A sink to write the detector triggers to.
     * @param event_sink A sink to write the events to.
     * @param supervisor A reference to a supervisor object, which keeps track of program metadata
     */
    station(sink::base<detetor_summary_t>& summary_sink, sink::base<trigger::detector>& trigger_sink, sink::base<event_t>& event_sink, sink::base<timebase_t>& timebase_sink, supervision::state& supervisor);

    /**
     * @brief detector_status Update the status of one detector
     * @param hash The hashed detector identifier
     * @param status The new status of the detector
     */
    void detector_status(std::size_t hash, detector_station::Status status);

    void get(trigger::detector::action_t action) override;

    void get(event_t event) override;

    void get(detetor_info_t<location_t> detector_info) override;

protected:
    /**
     * @brief process Process a log message. Hands the message over to a detector, if none exists, creates a new one.
     * @param log The log message to check
     */
    [[nodiscard]] auto process(detetor_info_t<location_t> log) -> int override;
    [[nodiscard]] auto process() -> int override;

    void save();
    void load();

private:
    supervision::state& m_supervisor;

    std::map<std::size_t, std::unique_ptr<detector_station>> m_detectors {};

    std::queue<std::size_t> m_delete_detectors {};

    std::chrono::steady_clock::time_point m_last { std::chrono::steady_clock::now() };

    std::map<std::size_t, std::map<trigger::detector::setting_t::Type, trigger::detector::setting_t>> m_detector_triggers {};
};

}
}

#endif // STATIONSUPERVISION_H
