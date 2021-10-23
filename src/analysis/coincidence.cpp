#include "analysis/coincidence.h"
#include "messages/event.h"

#include <muonpi/gnss.h>

#include <cmath>

#include <chrono>

namespace muonpi {

coincidence::~coincidence() = default;

auto coincidence::compare(const event_t::data_t& first, const event_t::data_t& second) const -> double
{
    const double delta { static_cast<double>(std::abs(first.start - second.start)) };
    if (delta > s_maximum_time) {
        return -1.0;
    }
    const coordinate::geodetic<double> first_c { first.location.lat * units::degree, first.location.lon * units::degree, first.location.h * units::meter };
    const coordinate::geodetic<double> second_c { second.location.lat * units::degree, second.location.lon * units::degree, second.location.h * units::meter };

    const auto distance { coordinate::transformation<double, coordinate::WGS84>::straight_distance(first_c, second_c) };
    const double time_of_flight { std::max(distance / consts::c_0, s_minimum_time) };

    return std::max(1.0 - delta / time_of_flight, -1.0);
}

} // namespace muonpi
