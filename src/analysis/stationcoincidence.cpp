#include "analysis/stationcoincidence.h"

#include "supervision/station.h"

#include <muonpi/gnss.h>
#include <muonpi/log.h>
#include <muonpi/units.h>
#include <muonpi/utility.h>

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace muonpi {

station_coincidence::station_coincidence(std::string data_directory, supervision::station& stationsupervisor, configuration config)
    : thread_runner { "muon::coinc" }
    , m_stationsupervisor { stationsupervisor }
    , m_data_directory { std::move(data_directory) }
    , m_config { std::move(config) }
{
    reset();
    start();
}

auto station_coincidence::step() -> int
{
    std::mutex mx;
    std::unique_lock<std::mutex> lock { mx };
    m_condition.wait_for(lock, m_config.histogram_sample_time);
    if (!m_quit) {
        save();
    }
    return 0;
}

auto station_coincidence::post_run() -> int
{
    save();
    return 0;
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

void station_coincidence::get(trigger::detector trig)
{
    auto condition_1 = [&](const auto& iterator) { return iterator.first.hash() == trig.hash; };
    auto it_1 { std::find_if(m_stations.begin(), m_stations.end(), condition_1) };
    if (it_1 == m_stations.end()) {
        return;
    }
    std::size_t index { static_cast<std::size_t>(std::distance(m_stations.begin(), it_1)) };

    m_data.iterate(index, [&](data_t& data) {
        switch (trig.status) {
        case detector_status::unreliable:
            if (data.online == 2) {
                data.uptime += std::chrono::duration_cast<std::chrono::minutes>(std::chrono::system_clock::now() - data.last_online).count();
            }
            data.online--;
            break;
        case detector_status::reliable:
            if (data.online == 1) {
                data.last_online = std::chrono::system_clock::now();
            }
            data.online++;
            break;
        default:
            return;
        }
    });
}

void station_coincidence::save()
{
    const auto now { std::chrono::system_clock::now() };
    constexpr static double grace_factor { 0.9 };
    const auto duration { now - m_last_save };
    if (duration < (m_config.histogram_sample_time * grace_factor)) {
        log::warning("coincidence analysis") << "Last histogram store was too recent. Refusing to save now.";
        return;
    }
    if (!std::filesystem::exists(m_data_directory)) {
        std::filesystem::create_directories(m_data_directory);
    }
    const std::string filename { std::to_string(std::chrono::duration_cast<std::chrono::hours>(now.time_since_epoch()).count()) };

    m_last_save = now;
    log::debug("coincidence analysis") << "Saving histogram data.";
    m_saving = true;

    std::ofstream stationfile { m_data_directory + "/" + filename + ".stations" };

    std::map<std::size_t, userinfo_t> stations {};
    std::map<std::size_t, std::map<std::size_t, std::size_t>> station_matrix {};

    std::map<std::size_t, std::size_t> row_vector {};

    for (const auto& [userinfo, location] : m_stations) {
        row_vector.emplace(userinfo.hash(), 0);
    }

    for (const auto& [userinfo, location] : m_stations) {
        stations.emplace(userinfo.hash(), userinfo);

        station_matrix.emplace(userinfo.hash(), row_vector);

        stationfile << std::hex << userinfo.hash() << ';' << userinfo.site_id() << ';' << location.lat << ';' << location.lon << ';' << location.h << '\n';
    }
    stationfile.close();

    for (auto& data : m_data.data()) {
        if (data.online == 2) {
            data.uptime += std::chrono::duration_cast<std::chrono::minutes>(now - data.last_online).count();
            data.last_online = now;
        }

        station_matrix[data.first][data.second] = data.hist.integral();
        station_matrix[data.second][data.first] = data.hist.integral();

        std::ostringstream dir_stream {};
        dir_stream << m_data_directory << '/';
        std::string first_site { stations[data.first].site_id() };
        std::string second_site { stations[data.second].site_id() };

        std::replace(first_site.begin(), first_site.end(), '/', '-');
        std::replace(second_site.begin(), second_site.end(), '/', '-');

        if (data.first < data.second) {
            dir_stream << first_site
                       << "_"
                       << second_site;
        } else {
            dir_stream << second_site
                       << "_"
                       << first_site;
        }
        dir_stream << '/';
        if (!std::filesystem::exists(dir_stream.str())) {
            std::filesystem::create_directories(dir_stream.str());
        }
        dir_stream << filename;

        std::ofstream histogram_file { dir_stream.str() + ".hist" };
        for (const auto& bin : data.hist.qualified_bins()) {
            histogram_file << ((bin.lower + bin.upper) / 2) << ' ' << bin.count << '\n';
        }
        histogram_file.close();

        std::ofstream metadata_file { dir_stream.str() + ".meta" };
        metadata_file
            << "bin_width " << std::to_string(data.hist.width()) << " ns\n"
            << "distance " << data.distance << " m\n"
            << "total " << std::to_string(data.hist.integral()) << " 1\n"
            << "uptime " << std::to_string(data.uptime) << " min\n"
            << "sample_time " << std::to_string(std::chrono::duration_cast<std::chrono::minutes>(duration).count()) << "min\n";
        metadata_file.close();
        data.uptime = 0;
        data.hist.reset();
    }
    std::ofstream adjacentfile { m_data_directory + "/" + filename + ".adj" };
    for (const auto& [hash, row] : row_vector) {
        adjacentfile << ';' << std::hex << hash;
    }
    adjacentfile << '\n';
    for (const auto& [hash, row] : station_matrix) {
        adjacentfile << std::hex << hash;
        for (const auto& [inner_hash, n] : row) {
            adjacentfile << ';' << std::dec << n;
        }
        adjacentfile << '\n';
    }
    adjacentfile.close();
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
        coordinate::geodetic<double> first { location.lat * units::degree, location.lon * units::degree, location.h * units::meter };
        for (std::size_t y { 0 }; y < x; y++) {
            const auto& [user, loc] { m_stations.at(y) };
            const auto distance { coordinate::transformation<double, coordinate::WGS84>::straight_distance(first, { loc.lat * units::degree, loc.lon * units::degree, loc.h }) };
            const auto time_of_flight { distance / consts::c_0 };
            const std::int32_t bin_width { static_cast<std::int32_t>(std::clamp((2.0 * time_of_flight) / static_cast<double>(s_bins), 1.0, s_total_width / static_cast<double>(s_bins))) };
            const std::int32_t min { bin_width * -static_cast<std::int32_t>(s_bins * 0.5) };
            const std::int32_t max { bin_width * static_cast<std::int32_t>(s_bins * 0.5) };
            m_data.emplace(x, y, { userinfo.hash(), user.hash(), static_cast<float>(distance), histogram_t { s_bins, min, max } });
        }
    }
}

} // namespace muonpi
