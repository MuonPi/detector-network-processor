#ifndef MQTTSINK_H
#define MQTTSINK_H

#include "link/mqtt.h"
#include "sink/base.h"

#include "utility/geohash.h"
#include "utility/log.h"
#include "utility/utility.h"

#include "messages/clusterlog.h"
#include "messages/detectorinfo.h"
#include "messages/detectorlog.h"
#include "messages/detectorsummary.h"
#include "messages/event.h"
#include "messages/trigger.h"

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
     */
    mqtt(link::mqtt::publisher& publisher, bool detailed = false);

    ~mqtt() override;

    void set_detailed();

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
void mqtt<T>::set_detailed()
{
    m_detailed = true;
}

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
    if (!(
            m_link.publish((construct(stream.str(), "timeout") << log.timeout).str())
            && m_link.publish((construct(stream.str(), "timebase") << log.timebase).str())
            && m_link.publish((construct(stream.str(), "uptime") << log.uptime).str())
            && m_link.publish((construct(stream.str(), "frequency_in") << log.frequency.single_in).str())
            && m_link.publish((construct(stream.str(), "frequency_l1_out") << log.frequency.l1_out).str())
            && m_link.publish((construct(stream.str(), "buffer_length") << log.buffer_length).str())
            && m_link.publish((construct(stream.str(), "total_detectors") << log.total_detectors).str())
            && m_link.publish((construct(stream.str(), "reliable_detectors") << log.reliable_detectors).str())
            && m_link.publish((construct(stream.str(), "max_coincidences") << log.maximum_n).str())
            && m_link.publish((construct(stream.str(), "cpu_load") << log.system_cpu_load).str())
            && m_link.publish((construct(stream.str(), "process_cpu_load") << log.process_cpu_load).str())
            && m_link.publish((construct(stream.str(), "memory_usage") << log.memory_usage).str())
            && m_link.publish((construct(stream.str(), "incoming") << log.incoming).str()))) {
        log::warning() << "Could not publish MQTT message.";
        return;
    }
    for (auto& [level, n] : log.outgoing) {
        if (level == 1) {
            continue;
        }
        if (!m_link.publish((construct(stream.str(), "outgoing_" + std::to_string(level)) << n).str())) {
            log::warning() << "Could not publish MQTT message.";
            return;
        }
    }
}

template <>
void mqtt<detector_summary_t>::get(detector_summary_t log)
{
    std::time_t time { std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) };
    std::ostringstream stream {};
    stream << std::put_time(std::gmtime(&time), "%F_%H-%M-%S");

    std::string name { log.userinfo.username + " " + log.userinfo.station_id };
    if (!(
            m_link.publish((construct(stream.str(), name + " eventrate") << log.mean_eventrate).str())
            && m_link.publish((construct(stream.str(), name + " eventrate_stddev") << log.stddev_eventrate).str())
            && m_link.publish((construct(stream.str(), name + " time_acc") << log.mean_time_acc).str())
            && m_link.publish((construct(stream.str(), name + " pulselength") << log.mean_pulselength).str())
            && m_link.publish((construct(stream.str(), name + " incoming") << log.incoming).str())
            && m_link.publish((construct(stream.str(), name + " ublox_counter_progess") << log.ublox_counter_progress).str())
            && m_link.publish((construct(stream.str(), name + " deadtime_factor") << log.deadtime).str()))) {
        log::warning() << "Could not publish MQTT message.";
    }
}

template <>
void mqtt<event_t>::get(event_t event)
{
    if (event.n() < 2) {
        return;
    }

    const std::int64_t cluster_coinc_time = event.end - event.start;
    guid uuid { event.hash, static_cast<std::uint64_t>(event.start) };
    for (auto& evt : event.events) {
        location_t loc = evt.location;
        // calculate the geohash up to 5 digits, this should avoid a precise tracking of the detector location
        std::string geohash = geohash::from_coordinates(loc.lon, loc.lat, loc.max_geohash_length);
        message_constructor message { ' ' };
        message.add_field(uuid.to_string()); // UUID for the L1Event
        message.add_field(int_to_hex(evt.hash)); // the hashed detector id
        message.add_field(geohash); // the geohash of the detector's location
        message.add_field(std::to_string(evt.time_acc)); // station's time accuracy
        message.add_field(std::to_string(event.n())); // event multiplicity (coinc level)
        message.add_field(std::to_string(cluster_coinc_time)); // total time span of the event (last - first)
        message.add_field(std::to_string(evt.start - event.start)); // relative time of the station within the event (referred to first detector hit)
        message.add_field(std::to_string(evt.ublox_counter)); // the station's hardware event counter (16bit)
        message.add_field(std::to_string(evt.duration())); // the pulse length of the station for the hit contributing to this event
        message.add_field(std::to_string(evt.gnss_time_grid)); // the time grid to which the station was synced at the moment of the event
        message.add_field(std::to_string(evt.fix)); // if the station had a valid GNSS fix at the time of the event
        message.add_field(std::to_string(evt.start)); // the timestamp of the stations hit
        message.add_field(std::to_string(evt.utc)); //if the station uses utc

        if (m_detailed) {
            if (!m_link.publish(evt.user + "/" + evt.station_id, message.get_string())) {
                log::warning() << "Could not publish MQTT message.";
            }
        } else {
            if (!m_link.publish(message.get_string())) {
                log::warning() << "Could not publish MQTT message.";
            }
        }
    }
}

template <>
void mqtt<trigger::detector>::get(trigger::detector trigger)
{
    std::time_t time { std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) };
    std::ostringstream stream {};
    stream
        << std::put_time(std::gmtime(&time), "%F_%H-%M-%S %Z");
    switch (trigger.setting.type) {
    case trigger::detector::setting_t::Offline:
        stream << " offline";
        break;
    case trigger::detector::setting_t::Online:
        stream << " online";
        break;
    case trigger::detector::setting_t::Unreliable:
        stream << " unreliable";
        break;
    case trigger::detector::setting_t::Reliable:
        stream << " reliable";
        break;
    case trigger::detector::setting_t::Invalid:
        return;
    }

    if (!m_link.publish(trigger.setting.username + "/" + trigger.setting.station, stream.str())) {
        log::warning() << "Could not publish MQTT message.";
    }
}

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

template <>
void mqtt<detector_log_t>::get(detector_log_t log)
{
    std::time_t time { std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) };
    std::ostringstream stream {};
    stream << std::put_time(std::gmtime(&time), "%F_%H-%M-%S");

    while (!log.items.empty()) {
        detector_log_t::item item { log.get() };
        auto constr { construct(stream.str(), item.name) };
        std::visit(overloaded {
                       [&](std::string value) { constr << value; },
                       [&](std::int_fast64_t value) { constr << value; },
                       [&](std::size_t value) { constr << value; },
                       [&](std::uint8_t value) { constr << value; },
                       [&](std::uint16_t value) { constr << value; },
                       [&](std::uint32_t value) { constr << value; },
                       [&](bool value) { constr << value; },
                       [&](double value) { constr << value; } },
            item.value);
        if (!item.unit.empty()) {
            constr << item.unit;
        }
        if (!m_link.publish(log.userinfo.username + "/" + log.userinfo.station_id, constr.str())) {
            log::warning() << "Could not publish MQTT message.";
            return;
        }
    }
}

}

#endif // MQTTSINK_H
