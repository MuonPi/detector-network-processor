#ifndef DETECTORSTATION_H
#define DETECTORSTATION_H

#include "defaults.h"
#include "messages/detectorinfo.h"
#include "messages/detectorsummary.h"
#include "messages/userinfo.h"
#include "utility/threadrunner.h"

#include "analysis/dataseries.h"
#include "analysis/ratemeasurement.h"

#include <chrono>
#include <future>
#include <memory>

namespace muonpi {

// +++ forward declarations
struct event_t;
namespace supervision {
class station;
}
// --- forward declarations

/**
 * @brief The detector class
 * Represents a connected detector.
 * Scans the message rate and deletes the runtime objects from memory if the detector has not been active for some time.
 */
class detector_station {
private:
    static constexpr std::size_t s_history_length { 10 };
    static constexpr std::size_t s_time_interval { 30000 };

public:

    enum class Status {
        Created,
        Deleted,
        Unreliable,
        Reliable
    };

    void enable();
    /**
     * @brief detector
     * @param initial_log The initial log message from which this detector object originates
     */
    detector_station(const detector_info_t<location_t>& initial_log, supervision::station& stationsupervisor);

    /**
     * @brief detector Construct the detector from a serialised string
     * @param serialised The serialised string
     * @param stationsupervisor The station supervisor to use
     * @param stale whether the configuration is stale or not. If true, this detector will be marked as unreliable
     */
    detector_station(const std::string& serialised, supervision::station& stationsupervisor, bool stale);

    /**
     * @brief process Processes an event message. This means it calculates the event rate from this detector.
     * @param event the event to process
     * @return true if the event is accepted, false if not.
     */
    [[nodiscard]] auto process(const event_t& event) -> bool;

    /**
     * @brief process Processes a detector info message. Checks for regular log messages and warns the event listener if they are delayed or have subpar location accuracy.
     * @param info The detector info to process
     */
    void process(const detector_info_t<location_t>& info);

    /**
     * @brief is Checks the current detector status against a value
     * @param status The status to compare against
     * @return true if the detector has the status asked for in the parameter
     */
    [[nodiscard]] auto is(Status status) const -> bool;

    /**
     * @brief factor The current factor from the event supervisor
     * @return the numeric factor
     */
    [[nodiscard]] auto factor() const -> double;

    /**
     * @brief step Gets called by the supervision::station with a guaranteed maximum time delay. May be called more often.
     */
    void step();

    /**
     * @brief current_log_data gets the current log data.
     * @return
     */
    [[nodiscard]] auto current_log_data() -> detector_summary_t;

    /**
     * @brief change_log_data gets the current log data when the detector has experienced a change in status.
     * @return
     */
    [[nodiscard]] auto change_log_data() -> detector_summary_t;

    /**
     * @brief user_info Accesses the user info from the object
     * @return the userinfo_t struct
     */
    [[nodiscard]] auto user_info() const -> userinfo_t;

    /**
     * @brief location Accesses the location info of the detector
     * @return the location_t struct
     */
    [[nodiscard]] auto location() const -> location_t { return m_location; }

    /**
     * @brief serialise Serialise the current detector information
     * @return Serialised string
     */
    [[nodiscard]] auto serialise() const -> std::string;

protected:
    /**
     * @brief set_status Sets the status of this detector and sends the status to the listener if it has changed.
     * @param status The status to set
     */
    void set_status(Status status);

private:
    void check_reliability();

    Status m_status { Status::Unreliable };

    bool m_initial { true };

    location_t m_location {};
    std::size_t m_hash { 0 };
    userinfo_t m_userinfo {};

    std::chrono::system_clock::time_point m_last_log { std::chrono::system_clock::now() };

    static constexpr std::chrono::system_clock::duration s_log_interval { std::chrono::seconds { 90 } };
    static constexpr std::chrono::system_clock::duration s_quit_interval { s_log_interval * 3 };

    supervision::station& m_stationsupervisor;

    rate_measurement<s_history_length, s_time_interval> m_current_rate {};
    rate_measurement<s_history_length * 10, s_time_interval> m_mean_rate {};

    detector_summary_t m_current_data;
    std::uint16_t m_last_ublox_counter {};

    data_series<double, 100> m_pulselength {};
    data_series<double, 100> m_time_acc {}; //< ring buffer for time accuracy values provided by event messages (in ns)
    data_series<double, 5> m_reliability_time_acc {}; //< ring buffer for time accuracy for use as reliability measure

    double m_factor { 1.0 };
};

}

#endif // DETECTORSTATION_H
