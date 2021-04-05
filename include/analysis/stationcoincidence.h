#ifndef STATION_COINCIDENCE_H
#define STATION_COINCIDENCE_H

#include "utility/threadrunner.h"

#include "messages/event.h"
#include "messages/trigger.h"

#include "sink/base.h"

#include "analysis/uppermatrix.h"
#include "analysis/histogram.h"

#include <string>

namespace muonpi {

namespace supervision {
class station;
}

class station_coincidence : public sink::base<event_t>, public thread_runner
{
public:
    station_coincidence(std::string data_directory, supervision::station& stationsupervisor);

    void get(event_t event) override;

protected:
    [[nodiscard]] auto step() -> int override;

    void on_stop() override;

private:
    void save();
    void reset();
    void add_station(const userinfo_t& userinfo, const location_t& location);

    supervision::station& m_stationsupervisor;

    std::string m_data_directory {};

    constexpr static std::chrono::duration s_sample_time { std::chrono::hours{12} };
    constexpr static std::size_t s_bins { 2000 }; //<! total number of bins to use per pair
    constexpr static double s_c { 299'792'458.0 * 1.0e-9 };
    constexpr static double s_total_width { 2.0 * 100000.0 };

    std::condition_variable m_condition {};
    std::mutex m_mutex {};

    std::atomic<bool> m_saving { false };

    struct data_t {
        std::size_t first {};
        std::size_t second {};
        histogram<s_bins, std::int32_t, std::uint16_t> hist {};
    };
    std::vector<std::pair<userinfo_t, location_t>> m_stations {};
    upper_matrix<data_t> m_data { 0 };
};

}

#endif // STATION_COINCIDENCE_H
