#ifndef MQTTSINK_H
#define MQTTSINK_H

#include "sink/base.h"
#include "link/mqtt.h"

#include "utility/log.h"
#include "utility/geohash.h"
#include "utility/utility.h"

#include "messages/clusterlog.h"
#include "messages/detectorsummary.h"
#include "messages/detectorinfo.h"
#include "messages/event.h"
#include "messages/trigger.h"

#include "detector.h"

#include <memory>
#include <string>
#include <ctime>
#include <iomanip>

namespace MuonPi::Sink {

template <typename T>
/**
 * @brief The Mqtt class
 */
class Mqtt : public Base<T>
{
public:
    /**
     * @brief Mqtt
     * @param publisher The topic from which the messages should be published
     */
    Mqtt(Link::Mqtt::Publisher& publisher);

    ~Mqtt() override;

    void get(T message) override;

private:
    class Constructor
    {
    public:
        Constructor(std::ostringstream stream)
            : m_stream { std::move(stream) }
        {}

        template<typename U>
        auto operator<<(U value) -> Constructor& {
            m_stream<<' '<<value;
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
};

template <typename T>
Mqtt<T>::Mqtt(Link::Mqtt::Publisher& publisher)
    : m_link { publisher }
{
}

template <typename T>
Mqtt<T>::~Mqtt() = default;


template <typename T>
auto Mqtt<T>::construct(const std::string& time, const std::string& parname) -> Constructor
{
    std::ostringstream stream{};

    stream<<time<<' '<<parname;

    return Constructor{ std::move(stream) };
}

template <>
void Mqtt<ClusterLog>::get(ClusterLog log)
{
    std::time_t time { std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) };
    std::ostringstream stream {};
    stream<<std::put_time(std::gmtime(&time), "%F_%H-%M-%S");
    if (!(
                m_link.publish((construct(stream.str(), "timeout")<<log.data().timeout).str())
                && m_link.publish((construct(stream.str(), "frequency_in")<<log.data().frequency.single_in).str())
                && m_link.publish((construct(stream.str(), "frequency_l1_out")<<log.data().frequency.l1_out).str())
                && m_link.publish((construct(stream.str(), "buffer_length")<<log.data().buffer_length).str())
                && m_link.publish((construct(stream.str(), "total_detectors")<<log.data().total_detectors).str())
                && m_link.publish((construct(stream.str(), "reliable_detectors")<<log.data().reliable_detectors).str())
                && m_link.publish((construct(stream.str(), "max_coincidences")<<log.data().maximum_n).str())
                && m_link.publish((construct(stream.str(), "incoming")<<log.data().incoming).str())
          )) {
        Log::warning()<<"Could not publish MQTT message.";
        return;
    }
    for (auto& [level, n]: log.data().outgoing) {
        if (level == 1) {
            continue;
        }
        if (!m_link.publish((construct(stream.str(), "outgoing_" + std::to_string(level))<<n).str())) {
            Log::warning()<<"Could not publish MQTT message.";
            return;
        }

    }
}

template <>
void Mqtt<DetectorSummary>::get(DetectorSummary log)
{
    std::time_t time { std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) };
    std::ostringstream stream {};
    stream<<std::put_time(std::gmtime(&time), "%F_%H-%M-%S");

    std::string name { log.user_info().username + " " + log.user_info().station_id};
    if (!(
                m_link.publish((construct(stream.str(), name + " eventrate")<<log.data().mean_eventrate).str())
                && m_link.publish((construct(stream.str(), name + " time_acc")<<log.data().mean_time_acc).str())
                && m_link.publish((construct(stream.str(), name + " pulselength")<<log.data().mean_pulselength).str())
                && m_link.publish((construct(stream.str(), name + " incoming")<<log.data().incoming).str())
                && m_link.publish((construct(stream.str(), name + " ublox_counter_progess")<<log.data().ublox_counter_progress).str())
                && m_link.publish((construct(stream.str(), name + " deadtime_factor")<<log.data().deadtime).str())
          )) {
        Log::warning()<<"Could not publish MQTT message.";
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
    GUID guid{event.hash(), static_cast<std::uint64_t>(event.start())};
    for (auto& evt: event.events()) {
        Location loc = evt.location();
        // calculate the geohash up to 5 digits, this should avoid a precise tracking of the detector location
        std::string geohash = GeoHash::hashFromCoordinates(loc.lon, loc.lat, 5);
        MessageConstructor message {' '};
        message.add_field(guid.to_string()); // UUID for the L1Event
        //message.add_field(evt.data().user); // user name
        //message.add_field(evt.data().station_id); // station (detector) name
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

        if (!m_link.publish(message.get_string())) {
                Log::warning()<<"Could not publish MQTT message.";
                return;
        }
    }
}

template <>
void Mqtt<DetectorTrigger>::get(DetectorTrigger trigger)
{
    std::time_t time { std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) };
    std::ostringstream stream {};
    stream
            <<std::put_time(std::gmtime(&time), "%F_%H-%M-%S %Z");
    switch (trigger.type) {
    case DetectorTrigger::Offline:
        stream<<" offline";
        break;
    case DetectorTrigger::Online:
        stream<<" online";
        break;
    case DetectorTrigger::Unreliable:
        stream<<" unreliable";
        break;
    case DetectorTrigger::Reliable:
        stream<<" reliable";
        break;
    }

    if (!m_link.publish(trigger.username + "/" + trigger.station, stream.str())) {
        Log::warning()<<"Could not publish MQTT message.";
    }
    Log::debug()<<"Published trigger";
}

}

#endif // MQTTSINK_H
