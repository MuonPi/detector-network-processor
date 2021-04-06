#include "analysis/stationcoincidence.h"

#include "supervision/station.h"

#include "utility/coordinatemodel.h"
#include "utility/utility.h"

#include "utility/log.h"
#include "utility/units.h"

#include <algorithm>
#include <fstream>

namespace muonpi {

station_coincidence::station_coincidence(std::string data_directory, supervision::station& stationsupervisor)
    : thread_runner { "muon::coinc" }
    , m_stationsupervisor { stationsupervisor }
    , m_data_directory { std::move(data_directory) }
{
    start();
    reset();
}

auto station_coincidence::step() -> int
{
    std::unique_lock<std::mutex> lock { m_mutex };
    if (m_condition.wait_for(lock, s_sample_time) == std::cv_status::no_timeout) {
        return 0;
    }
    log::debug() << "Saving histogram data.";
    save();
    return 0;
}

void station_coincidence::on_stop()
{
    m_condition.notify_all();
}

void station_coincidence::get(event_t event)
{
    if ((event.n() < 2) || (m_saving)) {
        return;
    }

    for (std::size_t i { 0 }; i < (event.n() - 1); i++) {
        const std::size_t first_h { event.events.at(i).hash };
        auto condition_1 = [&](const auto& iterator) { return iterator.first.hash() == first_h; };
        auto it_1 { std::find_if(m_stations.begin(), m_stations.end(), condition_1) };
        std::size_t first { static_cast<std::size_t>(std::distance(m_stations.begin(), it_1)) };
        if (it_1 == m_stations.end()) {
            const auto& [userinfo, location] { m_stationsupervisor.get_station(first_h) };
            add_station(userinfo, location);
            first = m_stations.size() - 1;
        }
        const auto first_t { event.events.at(i).start };
        for (std::size_t j { i + 1 }; j < event.n(); j++) {
            const std::size_t second_h { event.events.at(j).hash };
            auto condition_2 = [&](const auto& iterator) { return iterator.first.hash() == second_h; };
            auto it_2 { std::find_if(m_stations.begin(), m_stations.end(), condition_2) };
            std::size_t second { static_cast<std::size_t>(std::distance(m_stations.begin(), it_2)) };
            if (it_2 == m_stations.end()) {
                const auto& [userinfo, location] { m_stationsupervisor.get_station(second_h) };
                add_station(userinfo, location);
                second = m_stations.size() - 1;
            }
            const auto second_t { event.events.at(j).start };

            auto& pair { m_data.at(std::max(first, second), std::min(first, second)) };
            if (second_h > first_h) {
                pair.hist.add(static_cast<std::int32_t>(first_t - second_t));
            } else {
                pair.hist.add(static_cast<std::int32_t>(second_t - first_t));
            }
        }
    }
}

void station_coincidence::save()
{
    m_saving = true;
    std::map<std::size_t, userinfo_t> stations {};
    for (const auto& [userinfo, location] : m_stations) {
        stations.emplace(userinfo.hash(), userinfo);
    }

    for (const auto& data : m_data.data()) {
        std::ofstream histogram_file { m_data_directory + "/" + stations[data.first].site_id() + "_" + stations[data.second].site_id() + ".hist" };
        for (const auto& bin : data.hist.qualified_bins()) {
            histogram_file << ((bin.lower + bin.upper) / 2) << ' ' << bin.count << '\n';
        }
        histogram_file.close();
        std::ofstream metadata_file { m_data_directory + "/" + stations[data.first].site_id() + "_" + stations[data.second].site_id() + ".meta" };
        metadata_file
                << "bin_width " << std::to_string(data.hist.width())<<" ns\n"
                << "distance " << data.distance << " m\n"
                << "total " << std::to_string(data.hist.integral()) << " 1\n";
    }
    reset();
    m_saving = false;
}

void station_coincidence::reset()
{
    m_stations.clear();
    m_data.reset();

    for (const auto& [userinfo, location] : m_stationsupervisor.get_stations()) {
        add_station(userinfo, location);
    }
}

void station_coincidence::add_station(const userinfo_t& userinfo, const location_t& location)
{
    const auto x { m_data.increase() };
    m_stations.emplace_back(std::make_pair(userinfo, location));
    if (x > 0) {
        coordinate::geodetic<double> first { location.lat * units::degree, location.lon * units::degree, location.h };
        for (std::size_t y { 0 }; y < x; y++) {
            const auto& [user, loc] { m_stations.at(y) };
            const auto distance { coordinate::transformation<double, coordinate::WGS84>::straight_distance(first, { loc.lat * units::degree, loc.lon * units::degree, loc.h }) };
            const auto time_of_flight { distance / s_c };
            const std::int32_t bin_width { static_cast<std::int32_t>(std::clamp((2.0 * time_of_flight) / static_cast<double>(s_bins), 1.0, s_total_width / static_cast<double>(s_bins))) };
            const std::int32_t min { bin_width * -static_cast<std::int32_t>(s_bins * 0.5) };
            const std::int32_t max { bin_width * static_cast<std::int32_t>(s_bins * 0.5) };
            m_data.emplace(x, y, { userinfo.hash(), user.hash(), static_cast<float>(distance), histogram_t { min, max } });
        }
    }
}

}
