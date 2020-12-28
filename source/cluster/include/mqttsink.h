#ifndef MQTTSINK_H
#define MQTTSINK_H

#include "abstractsink.h"
#include "mqttlink.h"
#include "log.h"
#include "clusterlog.h"
#include "detectorsummary.h"
#include "event.h"

#include <memory>
#include <string>
#include <ctime>
#include <iomanip>

namespace MuonPi {


template <typename T>
/**
 * @brief The MqttSink class
 */
class MqttSink : public AbstractSink<T>
{
public:
    /**
     * @brief MqttSink
     * @param publisher The topic from which the messages should be published
     */
    MqttSink(MqttLink::Publisher& publisher);

    ~MqttSink() override;

protected:
    /**
     * @brief step implementation from ThreadRunner
     * @return zero if the step succeeded.
     */
    [[nodiscard]] auto step() -> int override;

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

    void process(T log);

    void fix_time();
    [[nodiscard]] auto construct(const std::string& parname) -> Constructor;
    [[nodiscard]] auto publish(Constructor &constructor) -> bool;

    std::chrono::system_clock::time_point m_time {};
    MqttLink::Publisher m_link;
};


template <typename T>
MqttSink<T>::MqttSink(MqttLink::Publisher& publisher)
    : AbstractSink<T>{}
    , m_link { std::move(publisher) }
{
    AbstractSink<T>::start();
}

template <typename T>
MqttSink<T>::~MqttSink() = default;

template <typename T>
auto MqttSink<T>::step() -> int
{
    if (AbstractSink<T>::has_items()) {
        process(AbstractSink<T>::next_item());
    }
    std::this_thread::sleep_for(std::chrono::microseconds{50});
    return 0;
}

template <typename T>
void MqttSink<T>::fix_time()
{
    m_time = std::chrono::system_clock::now();
}


template <typename T>
auto MqttSink<T>::construct(const std::string& parname) -> Constructor
{
    std::ostringstream stream{};

    std::time_t time { std::chrono::system_clock::to_time_t(m_time) };

    stream<<std::put_time(std::gmtime(&time), "%F_%H-%M-%S")<<' '<<parname;

    return Constructor{ std::move(stream) };
}

template <>
void MqttSink<ClusterLog>::process(ClusterLog log)
{
    fix_time();
    if (!(
                m_link.publish((construct("timeout")<<log.data().timeout).str())
                && m_link.publish((construct("frequency_in")<<log.data().frequency.single_in).str())
                && m_link.publish((construct("frequency_l1_out")<<log.data().frequency.l1_out).str())
                && m_link.publish((construct("buffer_length")<<log.data().buffer_length).str())
                && m_link.publish((construct("total_detectors")<<log.data().total_detectors).str())
                && m_link.publish((construct("reliable_detectors")<<log.data().reliable_detectors).str())
                && m_link.publish((construct("max_coincidences")<<log.data().maximum_n).str())
                && m_link.publish((construct("incoming")<<log.data().incoming).str())
          )) {
        Log::warning()<<"Could not publish MQTT message.";
        return;
    }
    for (auto& [level, n]: log.data().outgoing) {
        if (level == 1) {
            continue;
        }
        if (!m_link.publish((construct("outgoing_" + std::to_string(level))<<n).str())) {
            Log::warning()<<"Could not publish MQTT message.";
            return;
        }

    }
}

template <>
void MqttSink<DetectorSummary>::process(DetectorSummary log)
{
    fix_time();
    std::string name { log.user_info().username + " " + log.user_info().station_id};
    if (!(
                m_link.publish((construct(name + " eventrate")<<log.data().mean_eventrate).str())
				&& m_link.publish((construct(name + " time_acc")<<log.data().mean_time_acc).str())
				&& m_link.publish((construct(name + " pulselength")<<log.data().mean_pulselength).str())
                && m_link.publish((construct(name + " incoming")<<log.data().incoming).str())
                && m_link.publish((construct(name + " ublox_counter_progess")<<log.data().ublox_counter_progress).str())
                && m_link.publish((construct(name + " deadtime_factor")<<log.data().deadtime).str())
          )) {
        Log::warning()<<"Could not publish MQTT message.";
    }
}

template <>
void MqttSink<Event>::process(Event event)
{
    if (event.n() == 1) {
        // by default, don't send out single events via MQTT
        return;
    }

    const std::int64_t cluster_coinc_time = event.end() - event.start();
    GUID guid{event.hash(), static_cast<std::uint64_t>(event.start())};
    for (auto& evt: event.events()) {
		MessageConstructor message {' '};
		message.add_field(guid.to_string()); // UUID for the L1Event
		message.add_field(evt.data().user); // user name
		message.add_field(evt.data().station_id); // station (detector) name
		message.add_field(std::to_string(evt.data().time_acc)); // station's time accuracy
		message.add_field(std::to_string(event.n())); // event multiplicity (coinc level)
		message.add_field(std::to_string(cluster_coinc_time)); // total time span of the event (last - first)
		message.add_field(std::to_string(evt.start() - event.start())); // relative time of the station within the event (referred to first detector hit)
		message.add_field(std::to_string(evt.data().ublox_counter)); // the station's hardware event counter (16bit)
		message.add_field(std::to_string(evt.duration())); // the pulse length of the station for the hit contributing to this event
		message.add_field(std::to_string(evt.data().gnss_time_grid)); // the time grid to which the station was synced at the moment of the event
		message.add_field(std::to_string(evt.data().fix)); // if the station had a valid GNSS fix at the time of the event
		message.add_field(std::to_string(evt.start())); // the timestamp of the station's hit
		if (!m_link.publish(message.get_string())) {
				Log::warning()<<"Could not publish MQTT message.";
				return;
		}
	}
}

}

#endif // MQTTSINK_H
/*
    if (event.n() == 1) {
        // by default, don't write the single events to the db
        return;
    }

    const std::int64_t cluster_coinc_time = event.end() - event.start();
    GUID guid{event.hash(), static_cast<std::uint64_t>(event.start())};
    for (auto& evt: event.events()) {
        bool result = m_link.measurement("L1Event")
                <<Influx::Tag{"user", evt.data().user}
                <<Influx::Tag{"detector", evt.data().station_id}
                <<Influx::Tag{"site_id", evt.data().user + evt.data().station_id}
                <<Influx::Field{"accuracy", evt.data().time_acc}
                <<Influx::Field{"uuid", guid.to_string()}
                <<Influx::Field{"coinc_level", event.n()}
                <<Influx::Field{"counter", evt.data().ublox_counter}
                <<Influx::Field{"length", evt.duration()}
                <<Influx::Field{"coinc_time", evt.start() - event.start()}
                <<Influx::Field{"cluster_coinc_time", cluster_coinc_time}
                <<Influx::Field{"time_ref", evt.data().gnss_time_grid}
                <<Influx::Field{"valid_fix", evt.data().fix}
                <<evt.start();


        if (!result) {
            Log::error()<<"Could not write event to database.";
            return;
        }
    }
    
*/

