#include "analysis/coincidence.h"
#include "messages/event.h"
#include "utility/coordinatemodel.h"

#include <cmath>

#include <chrono>

namespace muonpi {

coincidence::~coincidence() = default;

auto coincidence::apply(const event_t& first, const event_t& second) const -> double
{
    std::vector<event_t::data_t> first_data {};
    std::vector<event_t::data_t> second_data {};

    if (first.n() < 2) {
        first_data.emplace_back(first.data);
    } else {
        first_data = first.events;
    }

    if (second.n() < 2) {
        second_data.emplace_back(second.data);
    } else {
        second_data = second.events;
    }

    double sum {};

    for (const auto& data_f : first_data) {
        for (const auto& data_s : second_data) {
            sum += compare(data_f, data_s);
        }
    }

    return sum;
}

auto coincidence::compare(const event_t::data_t& first, const event_t::data_t& second) -> double
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
