#ifndef EVENT_H
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
    std::vector<event_t> events {};

    void emplace(event_t event) noexcept;
    [[nodiscard]] auto n() const noexcept -> std::size_t;
    [[nodiscard]] auto duration() const noexcept -> std::int_fast64_t;
};

}

#endif // EVENT_H
