﻿#ifndef EVENT_H
#define EVENT_H

#include "messages/detectorinfo.h"
#include "messages/userinfo.h"

#include <chrono>
#include <string>
#include <vector>

namespace muonpi {

struct timebase_t {
    double factor { 0.0 };
    std::chrono::steady_clock::duration base {};
};

struct event_t {
    struct data_t {
        location_t location {};
        userinfo_t userinfo {};
        std::uint64_t hash {};
        std::string user {};
        std::string station_id {};
        std::int_fast64_t start {};
        std::int_fast64_t end {};
        std::uint32_t time_acc {};
        std::uint16_t ublox_counter {};
        std::uint8_t fix {};
        std::uint8_t utc {};
        std::uint8_t gnss_time_grid {};
        [[nodiscard]] inline auto duration() const noexcept -> std::int_fast64_t
        {
            return end - start;
        }
    } data;

    std::vector<data_t> events {};

    /**
     * @brief emplace add an event to this event
     * @param event the event to add
     */
    void emplace(event_t event) noexcept;

    /**
     * @brief emplace add event data to this event
     * @param event the data to add
     */
    void emplace(data_t event) noexcept;

    /**
     * @brief n
     * @return the number of events represented by this object
     */
    [[nodiscard]] auto n() const noexcept -> std::size_t;

    /**
     * @brief duration the duration of the event. For coincidences represents the coincidence time.
     * @return the duration of the event
     */
    [[nodiscard]] auto duration() const noexcept -> std::int_fast64_t;
};

}

#endif // EVENT_H
