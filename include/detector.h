#ifndef DETECTOR_H
#define DETECTOR_H

#include "defaults.h"
#include "messages/detectorinfo.h"
#include "messages/detectorsummary.h"
#include "messages/userinfo.h"
#include "utility/threadrunner.h"
#include "utility/utility.h"

#include <chrono>
#include <future>
#include <memory>

namespace muonpi {

// +++ forward declarations
class Event;
class StateSupervisor;
class detector_tracker;
// --- forward declarations

/**
 * @brief The Detector class
 * Represents a connected detector.
 * Scans the message rate and deletes the runtime objects from memory if the detector has not been active for some time.
 */
class Detector {
private:
    static constexpr std::size_t s_history_length { 10 };
    static constexpr std::size_t s_time_interval { 30000 };

public:
    using CurrentRateType = RateMeasurement<s_history_length, s_time_interval>;
    using MeanRateType = RateMeasurement<s_history_length * 10, s_time_interval>;

    enum class Status {
        Created,
        Deleted,
        Unreliable,
        Reliable
    };

    void enable();
    /**
     * @brief Detector
     * @param initial_log The initial log message from which this detector object originates
     */
    Detector(const DetectorInfo<Location>& initial_log, detector_tracker& tracker);

    /**
     * @brief Detector Construct the detector from a serialised string
     * @param serialised The serialised string
     * @param tracker The detector tracker to use
     * @param stale whether the configuration is stale or not. If true, this detector will be marked as unreliable
     */
    Detector(const std::string& serialised, detector_tracker& tracker, bool stale);

    /**
     * @brief process Processes an event message. This means it calculates the event rate from this detector.
     * @param event the event to process
     * @return true if the event is accepted, false if not.
     */
    [[nodiscard]] auto process(const Event& event) -> bool;

    /**
     * @brief process Processes a detector info message. Checks for regular log messages and warns the event listener if they are delayed or have subpar location accuracy.
     * @param info The detector info to process
     */
    void process(const DetectorInfo<Location>& info);

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

    void step();

    [[nodiscard]] auto current_log_data() -> DetectorSummary;

    [[nodiscard]] auto change_log_data() -> DetectorSummary;

    /**
     * @brief user_info Accesses the user info from the object
     * @return the UserInfo struct
     */
    [[nodiscard]] auto user_info() const -> UserInfo;

    /**
     * @brief location Accesses the location info of the detector
     * @return the Location struct
     */
    [[nodiscard]] auto location() const -> Location { return m_location; }

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

    Location m_location {};
    std::size_t m_hash { 0 };
    UserInfo m_userinfo {};

    std::chrono::system_clock::time_point m_last_log { std::chrono::system_clock::now() };

    static constexpr std::chrono::system_clock::duration s_log_interval { std::chrono::seconds { 90 } };
    static constexpr std::chrono::system_clock::duration s_quit_interval { s_log_interval * 3 };

    detector_tracker& m_detectortracker;

    CurrentRateType m_current_rate {};
    MeanRateType m_mean_rate {};

    DetectorSummary::Data m_current_data;
    std::uint16_t m_last_ublox_counter {};

    Ringbuffer<double, 100> m_pulselength {};
    Ringbuffer<double, 100> m_time_acc {}; //< ring buffer for time accuracy values provided by event messages (in ns)
    Ringbuffer<double, 5> m_reliability_time_acc {}; //< ring buffer for time accuracy for use as reliability measure

    double m_factor { 1.0 };
};

}

#endif // DETECTOR_H
