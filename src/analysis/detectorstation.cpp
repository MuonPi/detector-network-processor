#include "analysis/detectorstation.h"
#include "messages/event.h"
#include "supervision/state.h"
#include "utility/log.h"
#include "utility/units.h"
#include "utility/utility.h"

#include "supervision/station.h"

namespace muonpi {

constexpr double max_timing_error { 1000.0 * units::nanosecond }; //< max allowable timing error in nanoseconds
constexpr double max_location_error { max_timing_error * consts::c_0 }; //< max allowable location error in meter
constexpr double extreme_timing_error { max_timing_error * 100.0 };
constexpr double stddev_factor { 0.75 };

void detector_station::enable()
{
    set_status(detector_status::created);
}

detector_station::detector_station(const detector_info_t<location_t>& initial_log, supervision::station& stationsupervisor)
    : m_location { initial_log.get<location_t>() }
    , m_hash { initial_log.hash }
    , m_userinfo { initial_log.userinfo }
    , m_stationsupervisor { stationsupervisor }
{
}

auto detector_station::process(const event_t& event) -> bool
{
    m_current_rate.increase_counter();
    m_mean_rate.increase_counter();
    m_current_data.incoming++;

    const std::uint16_t current_ublox_counter = event.data.ublox_counter;
    if (!m_initial) {
        std::uint16_t difference { static_cast<uint16_t>(current_ublox_counter - m_last_ublox_counter) };

        if (current_ublox_counter <= m_last_ublox_counter) {
            difference = current_ublox_counter + (std::numeric_limits<std::uint16_t>::max() - m_last_ublox_counter);
        }
        m_current_data.ublox_counter_progress += difference;
    } else {
        m_initial = false;
    }
    m_last_ublox_counter = current_ublox_counter;

    double pulselength { static_cast<double>(event.data.end - event.data.start) };
    if ((pulselength > 0.0) && (pulselength < units::mega)) {
        m_pulselength.add(pulselength);
    }
    m_time_acc.add(event.data.time_acc);
    m_reliability_time_acc.add(event.data.time_acc);

    if (event.data.time_acc > (extreme_timing_error)) {
        set_status(detector_status::unreliable, detector_status::reason::time_accuracy_extreme);
    }

    return (event.data.time_acc <= max_timing_error) && (event.data.fix == 1);
}

void detector_station::process(const detector_info_t<location_t>& info)
{
    m_last_log = std::chrono::system_clock::now();
    m_location = info.get<location_t>();
    check_reliability();
}

void detector_station::set_status(detector_status::status status, detector_status::reason reason)
{
    if (m_status != status) {
        m_stationsupervisor.on_detector_status(m_hash, status, reason);
    }
    m_status = status;
}

auto detector_station::is(detector_status::status status) const -> bool
{
    return m_status == status;
}

auto detector_station::factor() const -> double
{
    return m_factor;
}

void detector_station::check_reliability()
{
    constexpr static double hysteresis { 0.15 };

    const double loc_precision { m_location.dop * std::sqrt((m_location.h_acc * m_location.h_acc + m_location.v_acc * m_location.v_acc)) };
    const double f_location { loc_precision / max_location_error };
    const double f_time { m_reliability_time_acc.mean() / max_timing_error };
    const double f_rate { m_mean_rate.stddev() / (m_mean_rate.mean() * stddev_factor) };

    if (f_location > (1.0 + hysteresis)) {
        set_status(detector_status::unreliable, detector_status::reason::location_precision);
    } else if (f_time > (1.0 + hysteresis)) {
        set_status(detector_status::unreliable, detector_status::reason::time_accuracy);
    } else if (f_rate > (1.0 + hysteresis)) {
        set_status(detector_status::unreliable, detector_status::reason::rate_unstable);
    } else if ((f_location < (1.0 - hysteresis)) && (f_time < (1.0 - hysteresis)) && ((f_rate < (1.0 - hysteresis)))) {
        set_status(detector_status::reliable);
    }
}

void detector_station::step(const std::chrono::system_clock::time_point& now)
{
    auto diff { now - std::chrono::system_clock::time_point { m_last_log } };
    if (diff > s_log_interval) {
        if (diff > s_quit_interval) {
            set_status(detector_status::deleted, detector_status::reason::missed_log_interval);
            return;
        }
        set_status(detector_status::unreliable, detector_status::reason::missed_log_interval);

    } else {
        check_reliability();
    }

    if (m_current_rate.step(now)) {
        m_mean_rate.step(now);
        if (m_current_rate.mean() < (m_mean_rate.mean() - m_mean_rate.stddev())) {
            constexpr static double scale { 2.0 };
            m_factor = ((m_mean_rate.mean() - m_current_rate.mean()) / (m_mean_rate.stddev()) + 1.0) * scale;
        } else {
            m_factor = 1.0;
        }
    }
}

auto detector_station::current_log_data() -> detector_summary_t
{
    m_current_data.mean_eventrate = m_current_rate.mean();
    m_current_data.stddev_eventrate = m_current_rate.stddev();
    m_current_data.mean_pulselength = m_pulselength.mean();
    m_current_data.mean_time_acc = m_time_acc.mean();

    if (m_current_data.ublox_counter_progress == 0) {
        m_current_data.deadtime = 1.;
    } else {
        m_current_data.deadtime = 1. - static_cast<double>(m_current_data.incoming) / static_cast<double>(m_current_data.ublox_counter_progress);
    }
    detector_summary_t log { m_current_data };
    log.hash = m_hash;
    log.userinfo = m_userinfo;
    m_current_data.incoming = 0;
    m_current_data.ublox_counter_progress = 0;
    return log;
}

auto detector_station::change_log_data() -> detector_summary_t
{
    auto summary { current_log_data() };
    summary.change = 1;
    return summary;
}

auto detector_station::user_info() const -> userinfo_t
{
    return m_userinfo;
}

auto detector_station::location() const -> location_t
{
    return m_location;
}

auto detector_status::to_string(status s) -> std::string
{
    switch (s) {
    case detector_status::created:
        return "online";
        break;
    case detector_status::deleted:
        return "offline";
        break;
    case detector_status::reliable:
        return "reliable";
        break;
    case detector_status::unreliable:
        return "unreliable";
        break;
    default:
        return "invalid";
        break;
    }
}

auto detector_status::to_string(reason r) -> std::string
{
    switch (r) {
    case detector_status::reason::location_precision:
        return "location_precision";
        break;
    case detector_status::reason::rate_unstable:
        return "rate_unstable";
        break;
    case detector_status::reason::time_accuracy:
        return "time_accuracy";
        break;
    case detector_status::reason::time_accuracy_extreme:
        return "time_accuracy_extreme";
        break;
    case detector_status::reason::missed_log_interval:
        return "missed_log_interval";
        break;
    default:
        return "miscellaneous";
        break;
    }
}

} // namespace muonpi
