#ifndef ASCIISINK_H
#define ASCIISINK_H

#include "messages/clusterlog.h"
#include "messages/detectorsummary.h"
#include "messages/event.h"
#include "messages/trigger.h"
#include "sink/base.h"
#include "utility/utility.h"

#include <iostream>
#include <memory>

namespace muonpi::sink {

template <typename T>
/**
 * @brief The asciisink class
 */
class ascii : public base<T> {
public:
    /**
     * @brief asciievent_tsink
     * @param a_ostream The stream to which the output should be written
     */
    ascii(std::ostream& a_ostream);

    ~ascii() override;

    /**
     * @brief get Reimplemented from sink::base
     * @param message
     */
    void get(T message) override;

private:
    std::ostream& m_ostream;
};

template <typename T>
ascii<T>::ascii(std::ostream& ostream)
    : m_ostream { ostream }
{
}

template <typename T>
ascii<T>::~ascii() = default;

template <>
void ascii<event_t>::get(event_t event)
{
    if (event.n() < 2) {
        return;
    }

    guid uuid { event.data.hash, static_cast<std::uint64_t>(event.data.start) };
    const std::int64_t cluster_coinc_time = event.duration();
    std::ostringstream out {};
    out << "Combined event_t: (" << event.n() << "): coinc_time: " << cluster_coinc_time;
    for (const auto& evt : event.events) {
        const std::int64_t evt_coinc_time = evt.start - event.data.start;
        out
            << "\n\t" << std::hex << uuid.to_string() << std::dec << ' ' << evt_coinc_time
            << ' ' << evt.user
            << ' ' << evt.station_id
            << ' ' << evt.start
            << ' ' << evt.duration()
            << ' ' << evt.time_acc
            << ' ' << evt.ublox_counter
            << ' ' << static_cast<std::uint16_t>(evt.fix)
            << ' ' << static_cast<std::uint16_t>(evt.utc)
            << ' ' << static_cast<std::uint16_t>(evt.gnss_time_grid);
    }

    out << '\n';

    m_ostream << out.str() << std::flush;
}

template <>
void ascii<cluster_log_t>::get(cluster_log_t log)
{
    std::ostringstream out {};

    out
        << "Cluster Log:"
        << "\n\ttimeout: " << log.timeout << " ms"
        << "\n\ttimebase: " << log.timebase << " ms"
        << "\n\tuptime: " << log.uptime << " min"
        << "\n\tin: " << log.frequency.single_in << " Hz"
        << "\n\tout: " << log.frequency.l1_out << " Hz"
        << "\n\tbuffer: " << log.buffer_length
        << "\n\tevents in interval: " << log.incoming
        << "\n\tcpu load: " << log.system_cpu_load
        << "\n\tprocess cpu load: " << log.process_cpu_load
        << "\n\tmemory usage: " << log.memory_usage
        << "\n\tout in interval: ";

    for (auto& [n, i] : log.outgoing) {
        out << "(" << n << ":" << i << ") ";
    }

    out
        << "\n\tdetectors: " << log.total_detectors << "(" << log.reliable_detectors << ")"
        << "\n\tmaximum n: " << log.maximum_n << '\n';

    m_ostream << out.str() << std::flush;
}

template <>
void ascii<detector_summary_t>::get(detector_summary_t log)
{
    std::ostringstream out {};

    out
        << "Detector Summary: " << log.userinfo.site_id()
        << "\n\teventrate: " << log.mean_eventrate
        << "\n\teventrate stddev: " << log.stddev_eventrate
        << "\n\tpulselength: " << log.mean_pulselength
        << "\n\tincoming: " << log.incoming
        << "\n\tublox counter progess: " << log.ublox_counter_progress
        << "\n\tdeadtime factor: " << log.deadtime
        << '\n';

    m_ostream << out.str() << std::flush;
}

template <>
void ascii<trigger::detector>::get(trigger::detector trigger)
{
    if (trigger.status == detector_status::invalid) {
        return;
    }

    std::ostringstream stream {};
    stream << trigger.userinfo.username << ' ' << trigger.userinfo.station_id
           << ' ' << detector_status::to_string(trigger.status)
           << ' ' << detector_status::to_string(trigger.reason);

    m_ostream << stream.str() << '\n'
              << std::flush;
}

}

#endif // ASCIISINK_H
