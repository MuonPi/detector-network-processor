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
    if (event.n() > 1) {
        guid uuid { event.hash(), static_cast<std::uint64_t>(event.start()) };
        const std::int64_t cluster_coinc_time = event.end() - event.start();
        std::ostringstream out {};
        out << "Combined event_t: (" << event.n() << "): coinc_time: " << cluster_coinc_time;
        for (const auto& evt : event.events()) {
            const std::int64_t evt_coinc_time = evt.start() - event.start();
            out
                << "\n\t" << uuid.to_string() << ' ' << evt_coinc_time
                << std::hex
                << ' ' << evt.data().user
                << ' ' << evt.data().station_id
                << ' ' << std::dec << evt.data().start
                << ' ' << (evt.data().end - evt.data().start)
                << ' ' << std::hex << evt.data().time_acc
                << ' ' << evt.data().ublox_counter
                << ' ' << static_cast<std::uint16_t>(evt.data().fix)
                << ' ' << static_cast<std::uint16_t>(evt.data().utc)
                << ' ' << static_cast<std::uint16_t>(evt.data().gnss_time_grid);
        }

        out << '\n';

        m_ostream << out.str() << std::flush;
    }
}

template <>
void ascii<cluster_log_t>::get(cluster_log_t log)
{
    auto data { log.data() };
    std::ostringstream out {};

    out
        << "Cluster Log:"
        << "\n\ttimeout: " << data.timeout << " ms"
        << "\n\ttimebase: " << data.timebase << " ms"
        << "\n\tuptime: " << data.uptime << " ms"
        << "\n\tin: " << data.frequency.single_in << " Hz"
        << "\n\tout: " << data.frequency.l1_out << " Hz"
        << "\n\tbuffer: " << data.buffer_length
        << "\n\tevents in interval: " << data.incoming
        << "\n\tcpu load: " << data.system_cpu_load
        << "\n\tprocess cpu load: " << data.process_cpu_load
        << "\n\tmemory usage: " << data.memory_usage
        << "\n\tout in interval: ";

    for (auto& [n, i] : data.outgoing) {
        out << "(" << n << ":" << i << ") ";
    }

    out
        << "\n\tdetectors: " << data.total_detectors << "(" << data.reliable_detectors << ")"
        << "\n\tmaximum n: " << data.maximum_n << '\n';

    m_ostream << out.str() << std::flush;
}

template <>
void ascii<detetor_summary_t>::get(detetor_summary_t log)
{
    auto data { log.data() };
    std::ostringstream out {};

    out
        << "Detector Summary: " << log.user_info().site_id()
        << "\n\teventrate: " << data.mean_eventrate
        << "\n\teventrate stddev: " << data.stddev_eventrate
        << "\n\tpulselength: " << data.mean_pulselength
        << "\n\tincoming: " << data.incoming
        << "\n\tublox counter progess: " << data.ublox_counter_progress
        << "\n\tdeadtime factor: " << data.deadtime
        << '\n';

    m_ostream << out.str() << std::flush;
}

template <>
void ascii<trigger::detector>::get(trigger::detector trigger)
{
    m_ostream << "trigger: " + trigger.setting.to_string(' ') + '\n'
              << std::flush;
}

}

#endif // ASCIISINK_H
