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

#include "detector.h"

#include <ctime>
#include <iomanip>
#include <memory>
#include <string>

namespace MuonPi::Sink {

template <typename T>
/**
 * @brief The Mqtt class
 */
class Mqtt : public Base<T> {
public:
    /**
     * @brief Mqtt
     * @param publisher The topic from which the messages should be published
     */
    Mqtt(Link::Mqtt::Publisher& publisher);

    ~Mqtt() override;

    void set_detailed();

    void get(T message) override;

private:
    class Constructor {
    public:
        Constructor(std::ostringstream stream)
            : m_stream { std::move(stream) }
        {
        }

        template <typename U>
        auto operator<<(U value) -> Constructor&
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

    [[nodiscard]] auto construct(const std::string& time, const std::string& parname) -> Constructor;

    Link::Mqtt::Publisher& m_link;

    bool m_detailed { false };
};

template <typename T>
Mqtt<T>::Mqtt(Link::Mqtt::Publisher& publisher)
    : m_link { publisher }
{
}

template <typename T>
Mqtt<T>::~Mqtt() = default;

template <typename T>
void Mqtt<T>::set_detailed()
{
    m_detailed = true;
}

template <typename T>
auto Mqtt<T>::construct(const std::string& time, const std::string& parname) -> Constructor
{
    std::ostringstream stream {};

    stream << time << ' ' << parname;

    return Constructor { std::move(stream) };
}

template <>
void Mqtt<ClusterLog>::get(ClusterLog log)
{
    std::time_t time { std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) };
    std::ostringstream stream {};
    stream << std::put_time(std::gmtime(&time), "%F_%H-%M-%S");
    if (!(
            m_link.publish((construct(stream.str(), "timeout") << log.data().timeout).str())
            && m_link.publish((construct(stream.str(), "frequency_in") << log.data().frequency.single_in).str())
            && m_link.publish((construct(stream.str(), "frequency_l1_out") << log.data().frequency.l1_out).str())
            && m_link.publish((construct(stream.str(), "buffer_length") << log.data().buffer_length).str())
            && m_link.publish((construct(stream.str(), "total_detectors") << log.data().total_detectors).str())
            && m_link.publish((construct(stream.str(), "reliable_detectors") << log.data().reliable_detectors).str())
            && m_link.publish((construct(stream.str(), "max_coincidences") << log.data().maximum_n).str())
            && m_link.publish((construct(stream.str(), "incoming") << log.data().incoming).str()))) {
        Log::warning() << "Could not publish MQTT message.";
        return;
    }
    for (auto& [level, n] : log.data().outgoing) {
        if (level == 1) {
            continue;
        }
        if (!m_link.publish((construct(stream.str(), "outgoing_" + std::to_string(level)) << n).str())) {
            Log::warning() << "Could not publish MQTT message.";
            return;
        }
    }
}

template <>
void Mqtt<DetectorSummary>::get(DetectorSummary log)
{
    std::time_t time { std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) };
    std::ostringstream stream {};
    stream << std::put_time(std::gmtime(&time), "%F_%H-%M-%S");

    std::string name { log.user_info().username + " " + log.user_info().station_id };
    if (!(
            m_link.publish((construct(stream.str(), name + " eventrate") << log.data().mean_eventrate).str())
            && m_link.publish((construct(stream.str(), name + " time_acc") << log.data().mean_time_acc).str())
            && m_link.publish((construct(stream.str(), name + " pulselength") << log.data().mean_pulselength).str())
            && m_link.publish((construct(stream.str(), name + " incoming") << log.data().incoming).str())
            && m_link.publish((construct(stream.str(), name + " ublox_counter_progess") << log.data().ublox_counter_progress).str())
            && m_link.publish((construct(stream.str(), name + " deadtime_factor") << log.data().deadtime).str()))) {
        Log::warning() << "Could not publish MQTT message.";
    }
}

template <>
void Mqtt<Event>::get(Event event)
{
    if (event.n() == 1) {
        // by default, don't send out single events via MQTT
        return;
    }

    const std::int64_t cluster_coinc_time = event.end() - event.start();
    GUID guid { event.hash(), static_cast<std::uint64_t>(event.start()) };
    for (auto& evt : event.events()) {
        Location loc = evt.location();
        // calculate the geohash up to 5 digits, this should avoid a precise tracking of the detector location
        std::string geohash = GeoHash::hashFromCoordinates(loc.lon, loc.lat, 5);
        MessageConstructor message { ' ' };
        message.add_field(guid.to_string()); // UUID for the L1Event
        message.add_field(int_to_hex(evt.hash())); // the hashed detector id
        message.add_field(geohash); // the geohash of the detector's location
        message.add_field(std::to_string(evt.data().time_acc)); // station's time accuracy
        message.add_field(std::to_string(event.n())); // event multiplicity (coinc level)
        message.add_field(std::to_string(cluster_coinc_time)); // total time span of the event (last - first)
        message.add_field(std::to_string(evt.start() - event.start())); // relative time of the station within the event (referred to first detector hit)
        message.add_field(std::to_string(evt.data().ublox_counter)); // the station's hardware event counter (16bit)
        message.add_field(std::to_string(evt.duration())); // the pulse length of the station for the hit contributing to this event
        message.add_field(std::to_string(evt.data().gnss_time_grid)); // the time grid to which the station was synced at the moment of the event
        message.add_field(std::to_string(evt.data().fix)); // if the station had a valid GNSS fix at the time of the event
        message.add_field(std::to_string(evt.start())); // the timestamp of the stations hit
        message.add_field(std::to_string(evt.data().utc)); //if the station uses utc

        if (m_detailed) {
            if (!m_link.publish(evt.data().user + "/" + evt.data().station_id, message.get_string())) {
                Log::warning() << "Could not publish MQTT message.";
            }
        } else {
            if (!m_link.publish(message.get_string())) {
                Log::warning() << "Could not publish MQTT message.";
            }
        }
    }
}

template <>
void Mqtt<Trigger::Detector>::get(Trigger::Detector trigger)
{
    std::time_t time { std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) };
    std::ostringstream stream {};
    stream
        << std::put_time(std::gmtime(&time), "%F_%H-%M-%S %Z");
    switch (trigger.setting.type) {
    case Trigger::Detector::Setting::Offline:
        stream << " offline";
        break;
    case Trigger::Detector::Setting::Online:
        stream << " online";
        break;
    case Trigger::Detector::Setting::Unreliable:
        stream << " unreliable";
        break;
    case Trigger::Detector::Setting::Reliable:
        stream << " reliable";
        break;
    case Trigger::Detector::Setting::Invalid:
        return;
    }

    if (!m_link.publish(trigger.setting.username + "/" + trigger.setting.station, stream.str())) {
        Log::warning() << "Could not publish MQTT message.";
    }
}

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;
template <>

void Mqtt<DetectorLog>::get(DetectorLog log)
{
    std::time_t time { std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) };
    std::ostringstream stream {};
    stream << std::put_time(std::gmtime(&time), "%F_%H-%M-%S");

    while (log.has_items()) {
        DetectorLogItem item { log.next_item() };
        auto constructor { construct(stream.str(), item.name) };
        std::visit(overloaded {
                       [&](std::string value) { constructor << value; },
                       [&](std::int_fast64_t value) { constructor << value; },
                       [&](std::size_t value) { constructor << value; },
                       [&](std::uint8_t value) { constructor << value; },
                       [&](std::uint16_t value) { constructor << value; },
                       [&](std::uint32_t value) { constructor << value; },
                       [&](bool value) { constructor << value; },
                       [&](double value) { constructor << value; } },
            item.value);
        if (!item.unit.empty()) {
            constructor << item.unit;
        }
        if (!m_link.publish(log.user_info().username + "/" + log.user_info().station_id, constructor.str())) {
            Log::warning() << "Could not publish MQTT message.";
            return;
        }
    }
}

}

#endif // MQTTSINK_H
