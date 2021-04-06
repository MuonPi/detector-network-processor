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

struct detector_summary_t;
}

namespace muonpi::supervision {
class state;

class station
    : public sink::threaded<detector_info_t<location_t>>,
      public source::base<detector_summary_t>,
      public source::base<trigger::detector>,
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
    station(sink::base<detector_summary_t>& summary_sink, sink::base<trigger::detector>& trigger_sink, sink::base<event_t>& event_sink, sink::base<timebase_t>& timebase_sink, supervision::state& supervisor);

    /**
     * @brief detector_status Update the status of one detector
     * @param hash The hashed detector identifier
     * @param status The new status of the detector
     */
    void detector_status(std::size_t hash, detector_station::Status status);

    /**
     * @brief get Reimplemented from sink::base
     * @param event
     */
    void get(event_t event) override;

    /**
     * @brief get Reimplemented from sink::base
     * @param detector_info
     */
    void get(detector_info_t<location_t> detector_info) override;

    /**
     * @brief get_stations Get the information for all detector station
     * @return
     */
    [[nodiscard]] auto get_stations() const -> std::vector<std::pair<userinfo_t, location_t>>;

    /**
     * @brief get_station Get the information for a specific detector station for a hash
     * @param hash
     * @return
     */
    [[nodiscard]] auto get_station(std::size_t hash) const -> std::pair<userinfo_t, location_t>;

protected:
    /**
     * @brief process Process a log message. Hands the message over to a detector, if none exists, creates a new one.
     * @param log The log message to check
     */
    [[nodiscard]] auto process(detector_info_t<location_t> log) -> int override;

    [[nodiscard]] auto process() -> int override;

    void save();
    void load();

private:
    supervision::state& m_supervisor;

    std::map<std::size_t, std::unique_ptr<detector_station>> m_detectors {};

    std::queue<std::size_t> m_delete_detectors {};

    std::chrono::steady_clock::time_point m_last { std::chrono::steady_clock::now() };
};

}

#endif // STATIONSUPERVISION_H
