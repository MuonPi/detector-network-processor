#include "detector.h"
#include "messages/event.h"
#include "utility/log.h"
#include "supervision/state.h"

#include "detectortracker.h"

namespace MuonPi {

constexpr double LIGHTSPEED { 0.299 }; //< velocity of light in m/ns
constexpr double MAX_TIMING_ERROR { 1000. }; //< max allowable timing error in nanoseconds	
constexpr double MAX_LOCATION_ERROR { MAX_TIMING_ERROR * LIGHTSPEED }; //< max allowable location error in meter
	
void Detector::enable()
{
    set_status(Status::Created);
}

Detector::Detector(const DetectorInfo<Location> &initial_log, DetectorTracker& tracker)
    : m_location { initial_log.item()}
    , m_hash { initial_log.hash() }
    , m_userinfo { initial_log.user_info() }
    , m_detector_tracker { tracker }
{
}

void Detector::process(const Event& event)
{
    m_current_rate.increase_counter();
    m_mean_rate.increase_counter();
    m_current_data.incoming++;

    const std::uint16_t current_ublox_counter = event.data().ublox_counter;
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

    double pulselength { static_cast<double>(event.data().end - event.data().start) };
    if ((pulselength > 0.) && (pulselength < 1e6)) {
        m_pulselength.add(pulselength);
    }
    m_time_acc.add(event.data().time_acc);
}

void Detector::process(const DetectorInfo<Location> &info)
{
    m_last_log = std::chrono::system_clock::now();
    m_location = info.item();
    check_reliability();
}

void Detector::set_status(Status status)
{
    if (m_status != status) {
        m_detector_tracker.detector_status(m_hash, status);
    }
    m_status = status;
}

auto Detector::is(Status status) const -> bool
{
    return m_status == status;
}

auto Detector::factor() const -> double
{
    return m_factor;
}

void Detector::check_reliability()
{
    const double loc_precision { m_location.dop*std::sqrt((m_location.h_acc * m_location.h_acc + m_location.v_acc * m_location.v_acc)) };
    if ((loc_precision > MAX_LOCATION_ERROR) || ( m_time_acc.mean() > MAX_TIMING_ERROR )) {
        set_status(Status::Unreliable);
    } else {
        set_status(Status::Reliable);
    }
}

void Detector::step()
{
    auto diff { std::chrono::system_clock::now() - std::chrono::system_clock::time_point { m_last_log } };
    if (diff > s_log_interval) {
        if (diff > s_quit_interval) {
            set_status(Status::Deleted);
            return;
        }             set_status(Status::Unreliable);
       
    } else {
        check_reliability();
    }

    if (m_current_rate.step()) {
        m_mean_rate.step();
        if (m_current_rate.mean() < (m_mean_rate.mean() - m_mean_rate.deviation())) {
            m_factor = ((m_mean_rate.mean() - m_current_rate.mean())/(m_mean_rate.deviation()) + 1.0 ) * 2.0;
        } else {
            m_factor = 1.0;
        }
    }
}

auto Detector::current_log_data() -> DetectorSummary
{
    m_current_data.mean_eventrate = m_current_rate.mean();
    m_current_data.mean_pulselength = m_pulselength.mean();
    m_current_data.mean_time_acc = m_time_acc.mean();

    if (m_current_data.ublox_counter_progress == 0) {
        m_current_data.deadtime=1.;
    } else {
        m_current_data.deadtime = 1.-static_cast<double>(m_current_data.incoming)/static_cast<double>(m_current_data.ublox_counter_progress);
    }
    DetectorSummary log(m_hash, m_userinfo, m_current_data);
    m_current_data.incoming = 0;
    m_current_data.ublox_counter_progress = 0;
    return log;
}

auto Detector::change_log_data() -> DetectorSummary
{
    auto summary { current_log_data() };
    summary.set_change_flag();
    return summary;
}


auto Detector::user_info() const -> UserInfo
{
    return m_userinfo;
}
}
