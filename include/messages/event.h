#ifndef EVENT_H
#define EVENT_H

#include "defaults.h"
#include "messages/detectorinfo.h"
#include "messages/userinfo.h"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace muonpi {

class detector;

struct timebase_t {
    double factor { 0.0 };
    std::chrono::steady_clock::duration base {};
};

/**
 * @brief The event_t class
 *  class, which is used as an interface for single events and combined events
 */
class event_t {
public:
    struct Data {
        std::string user {};
        std::string station_id {};
        std::int_fast64_t start {};
        std::int_fast64_t end {};
        std::uint32_t time_acc {};
        std::uint16_t ublox_counter {};
        std::uint8_t fix {};
        std::uint8_t utc {};
        std::uint8_t gnss_time_grid {};
    };

    event_t(std::size_t hash, Data data) noexcept;

    event_t(event_t event, bool foreign) noexcept;

    event_t() noexcept;

    virtual ~event_t() noexcept;

    void set_detector_info(location_t location, userinfo_t user);

    auto location() const -> location_t;

    auto user_info() const -> userinfo_t;

    /**
     * @brief start
     * @return The starting time of the event
     */
    [[nodiscard]] auto start() const noexcept -> std::int_fast64_t;

    /**
     * @brief duration
     * @return The duration of the event
     */
    [[nodiscard]] auto duration() const noexcept -> std::int_fast64_t;

    /**
     * @brief end
     * @return The end time of the event
     */
    [[nodiscard]] auto end() const noexcept -> std::int_fast64_t;

    /**
     * @brief hash
     * @return The hash of the detector for this event
     */
    [[nodiscard]] auto hash() const noexcept -> std::size_t;

    /**
     * @brief n
     * @return The number of events of this event. 1 for a default event
     */
    [[nodiscard]] auto n() const noexcept -> std::size_t;

    /**
      * @brief Get the list of events.
      * @return The list of events contained in this combined event
      */
    [[nodiscard]] auto events() const -> const std::vector<event_t>&;

    /**
     * @brief add_event Adds an event to the Combinedevent_t.
     * @param event The event to add. In the case that the abstract event is a combined event, the child events will be added instead of their combination.
     */
    void add_event(event_t event) noexcept;

    /**
     * @brief valid
     * @return true when the event is valid
     */
    [[nodiscard]] auto valid() const -> bool;

    /**
     * @brief data get the data
     * @return The data struct of the event
     */
    [[nodiscard]] auto data() const -> Data;

    /**
     * @brief set_data Sets the data for the event
     * @param data The new data to use
     */
    void set_data(const Data& data);

private:
    std::size_t m_n { 1 };
    std::vector<event_t> m_events {};
    std::uint64_t m_hash {};
    bool m_valid { true };

    Data m_data {};

    location_t m_location {};
    userinfo_t m_user_info {};
};
}

#endif // EVENT_H
