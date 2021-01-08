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

namespace MuonPi::Sink {

template <typename T>
/**
 * @brief The AsciiSink class
 */
class Ascii : public Base<T> {
public:
    /**
     * @brief AsciiEventSink
     * @param a_ostream The stream to which the output should be written
     */
    Ascii(std::ostream& a_ostream);

    ~Ascii() override;

    void get(T message) override;

private:
    std::ostream& m_ostream;
};

template <typename T>
Ascii<T>::Ascii(std::ostream& ostream)
    : m_ostream { ostream }
{
}

template <typename T>
Ascii<T>::~Ascii() = default;

template <>
void Ascii<Event>::get(Event event)
{
    if (event.n() > 1) {
        GUID guid { event.hash(), static_cast<std::uint64_t>(event.start()) };
        const std::int64_t cluster_coinc_time = event.end() - event.start();
        std::ostringstream out {};
        out << "Combined Event: (" << event.n() << "): coinc_time: " << cluster_coinc_time;
        for (const auto& evt : event.events()) {
            const std::int64_t evt_coinc_time = evt.start() - event.start();
            out
                << "\n\t" << guid.to_string() << ' ' << evt_coinc_time
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
        /*    } else {

        std::ostringstream out {};
        out << "Event"
            <<std::hex
            <<' '<<event.data().user
            <<' '<<event.data().station_id
            <<' '<<std::dec<<event.data().start
            <<' '<<(event.data().end - event.data().start)
            <<' '<<std::hex<<event.data().time_acc
            <<' '<<event.data().ublox_counter
            <<' '<<static_cast<std::uint16_t>(event.data().fix)
            <<' '<<static_cast<std::uint16_t>(event.data().utc)
            <<' '<<static_cast<std::uint16_t>(event.data().gnss_time_grid)
            <<'\n';

        m_ostream<<out.str()<<std::flush;*/
    }
}

template <>
void Ascii<ClusterLog>::get(ClusterLog log)
{
    auto data { log.data() };
    std::ostringstream out {};

    out
        << "Cluster Log:"
        << "\n\ttimeout: " << data.timeout << " ms"
        << "\n\tin: " << data.frequency.single_in << " Hz"
        << "\n\tout: " << data.frequency.l1_out << " Hz"
        << "\n\tbuffer: " << data.buffer_length
        << "\n\tevents in interval: " << data.incoming
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
void Ascii<DetectorSummary>::get(DetectorSummary log)
{
    auto data { log.data() };
    std::ostringstream out {};

    out
        << "Detector Log: " << log.user_info().site_id()
        << "\n\teventrate: " << data.mean_eventrate
        << "\n\tpulselength: " << data.mean_pulselength
        << "\n\tincoming: " << data.incoming
        << "\n\tublox counter progess: " << data.ublox_counter_progress
        << "\n\tdeadtime factor: " << data.deadtime
        << '\n';

    m_ostream << out.str() << std::flush;
}

template <>
void Ascii<Trigger::Detector>::get(Trigger::Detector trigger)
{
    m_ostream << "trigger: " + trigger.setting.to_string(' ') + '\n'
              << std::flush;
}

}

#endif // ASCIISINK_H
