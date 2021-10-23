#ifndef MQTTSINK_H
#define MQTTSINK_H

#include "messages/clusterlog.h"
#include "messages/detectorinfo.h"
#include "messages/detectorlog.h"
#include "messages/detectorsummary.h"
#include "messages/event.h"
#include "messages/trigger.h"

#include <muonpi/link/mqtt.h>
#include <muonpi/sink/base.h>

#include <muonpi/gnss.h>
#include <muonpi/log.h>
#include <muonpi/utility.h>
#include <muonpi/units.h>

#include <ctime>
#include <iomanip>
#include <memory>
#include <string>

namespace muonpi::sink {

template <typename T>
/**
 * @brief The mqtt class
 */
class mqtt : public base<T> {
public:
    /**
     * @brief mqtt
     * @param publisher The topic from which the messages should be published
     * @paragraph detailed if false, anonymises the users of mqtt messages
     */
    mqtt(link::mqtt::publisher& publisher, bool detailed = false);

    ~mqtt() override;

    /**
     * @brief get Reimplemented from sink::base
     * @param message
     */
    void get(T message) override;

private:
    class constructor {
    public:
        constructor(std::ostringstream stream)
            : m_stream { std::move(stream) }
        {
        }

        template <typename U>
        auto operator<<(U value) -> constructor&
        {
            m_stream << ' ' << value;
            return *this;
        }

        auto str() -> std::string
        {
            return m_stream.str();
        }

    private:
        std::ostringstream m_stream;
    };

    [[nodiscard]] auto construct(const std::string& time, const std::string& parname) -> constructor;

    link::mqtt::publisher& m_link;

    bool m_detailed { false };
};

template <typename T>
mqtt<T>::mqtt(link::mqtt::publisher& publisher, bool detailed)
    : m_link { publisher }
    , m_detailed { detailed }
{
}

template <typename T>
mqtt<T>::~mqtt() = default;

template <typename T>
auto mqtt<T>::construct(const std::string& time, const std::string& parname) -> constructor
{
    std::ostringstream stream {};

    stream << time << ' ' << parname;

    return constructor { std::move(stream) };
}

template <>
void mqtt<cluster_log_t>::get(cluster_log_t log)
{
    std::time_t time { std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) };
    std::ostringstream stream {};
    stream << std::put_time(std::gmtime(&time), "%F_%H-%M-%S");
    m_link.publish((construct(stream.str(), "timeout") << log.timeout).str());
    m_link.publish((construct(stream.str(), "version") << Version::dnp::string()).str());
    m_link.publish((construct(stream.str(), "timebase") << log.timebase).str());
    m_link.publish((construct(stream.str(), "uptime") << log.uptime).str());
    m_link.publish((construct(stream.str(), "frequency_in") << log.frequency.single_in).str());
    m_link.publish((construct(stream.str(), "frequency_l1_out") << log.frequency.l1_out).str());
    m_link.publish((construct(stream.str(), "buffer_length") << log.buffer_length).str());
    m_link.publish((construct(stream.str(), "total_detectors") << log.total_detectors).str());
    m_link.publish((construct(stream.str(), "reliable_detectors") << log.reliable_detectors).str());
    m_link.publish((construct(stream.str(), "max_coincidences") << log.maximum_n).str());
    m_link.publish((construct(stream.str(), "cpu_load") << log.system_cpu_load).str());
    m_link.publish((construct(stream.str(), "process_cpu_load") << log.process_cpu_load).str());
    m_link.publish((construct(stream.str(), "memory_usage") << log.memory_usage).str());
    m_link.publish((construct(stream.str(), "plausibility_level") << log.plausibility_level).str());
    m_link.publish((construct(stream.str(), "incoming") << log.incoming).str());

    for (auto& [level, n] : log.outgoing) {
        if (level == 1) {
            continue;
        }
        m_link.publish((construct(stream.str(), "outgoing_" + std::to_string(level)) << n).str());
    }
}

template <>
void mqtt<detector_summary_t>::get(detector_summary_t log)
{
    std::time_t time { std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) };
    std::ostringstream stream {};
    stream << std::put_time(std::gmtime(&time), "%F_%H-%M-%S");

    std::string name { log.userinfo.username + " " + log.userinfo.station_id };

    m_link.publish((construct(stream.str(), name + " eventrate") << log.mean_eventrate).str());
    m_link.publish((construct(stream.str(), name + " eventrate_stddev") << log.stddev_eventrate).str());
    m_link.publish((construct(stream.str(), name + " time_acc") << log.mean_time_acc).str());
    m_link.publish((construct(stream.str(), name + " pulselength") << log.mean_pulselength).str());
    m_link.publish((construct(stream.str(), name + " incoming") << log.incoming).str());
    m_link.publish((construct(stream.str(), name + " ublox_counter_progess") << log.ublox_counter_progress).str());
    m_link.publish((construct(stream.str(), name + " deadtime_factor") << log.deadtime).str());
}

template <>
void mqtt<event_t>::get(event_t event)
{
    if (event.n() < 2) {
        return;
    }

    const std::int64_t cluster_coinc_time = event.data.end - event.data.start;
    guid uuid { event.data.hash, static_cast<std::uint64_t>(event.data.start) };
    for (auto& evt : event.events) {
        location_t loc = evt.location;
        // calculate the geohash up to 5 digits, this should avoid a precise tracking of the detector location
        std::string geohash = coordinate::hash<double>::from_geodetic(coordinate::geodetic<double> { loc.lon * units::degree, loc.lat * units::degree }, loc.max_geohash_length);
        message_constructor message { ' ' };
        message.add_field(uuid.to_string()); // UUID for the L1Event
        std::stringstream ss;
        ss << std::setfill('0') << std::setw(sizeof(evt.hash) * 2) << std::hex << (evt.hash | 0);
        message.add_field(ss.str()); // the hashed detector id
        message.add_field(geohash); // the geohash of the detector's location
        message.add_field(std::to_string(evt.time_acc)); // station's time accuracy
        message.add_field(std::to_string(event.n())); // event multiplicity (coinc level)
        message.add_field(std::to_string(cluster_coinc_time)); // total time span of the event (last - first)
        message.add_field(std::to_string(evt.start - event.data.start)); // relative time of the station within the event (referred to first detector hit)
        message.add_field(std::to_string(evt.ublox_counter)); // the station's hardware event counter (16bit)
        message.add_field(std::to_string(evt.duration())); // the pulse length of the station for the hit contributing to this event
        message.add_field(std::to_string(evt.gnss_time_grid)); // the time grid to which the station was synced at the moment of the event
        message.add_field(std::to_string(evt.fix)); // if the station had a valid GNSS fix at the time of the event
        message.add_field(std::to_string(evt.start)); // the timestamp of the stations hit
        message.add_field(std::to_string(evt.utc)); //if the station uses utc
        message.add_field(event.conflicting ? "conflicting" : "valid"); // if the event is conflicting or not
        message.add_field(std::to_string(event.true_e)); // The number of true edges in the event graph

        if (m_detailed) {
            m_link.publish(evt.user + "/" + evt.station_id, message.get_string());
        } else {
            m_link.publish(message.get_string());
        }
    }
}

template <>
void mqtt<trigger::detector>::get(trigger::detector trigger)
{
    std::time_t time { std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) };
    std::ostringstream stream {};
    stream
        << std::put_time(std::gmtime(&time), "%F_%H-%M-%S %Z")
        << ' ' << detector_status::to_string(trigger.status)
        << ' ' << detector_status::to_string(trigger.reason);

    m_link.publish(trigger.userinfo.username + "/" + trigger.userinfo.station_id, stream.str());
}

template <>
void mqtt<detector_log_t>::get(detector_log_t log)
{
    std::time_t time { std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) };
    std::ostringstream stream {};
    stream << std::put_time(std::gmtime(&time), "%F_%H-%M-%S");

    while (!log.items.empty()) {
        detector_log_t::item item { log.get() };
        auto constr { construct(stream.str(), item.name) };
        if (item.type == detector_log_t::item::Type::Double) {
            constr << item.value_d;
        } else if (item.type == detector_log_t::item::Type::Int) {
            constr << item.value_i;
        } else {
            constr << item.value_s;
        }
        if (!item.unit.empty()) {
            constr << item.unit;
        }
        m_link.publish(log.userinfo.username + "/" + log.userinfo.station_id, constr.str());
    }
}

}

#endif // MQTTSINK_H
