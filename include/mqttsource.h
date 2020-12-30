#ifndef MQTTLOGSOURCE_H
#define MQTTLOGSOURCE_H

#include "abstractsource.h"
#include "mqttlink.h"
#include "detectorinfo.h"
#include "userinfo.h"
#include "utility.h"
#include "log.h"
#include "event.h"

#include <map>
#include <memory>

namespace MuonPi {

/**
 * @brief The MqttSource class
 */
template <class T>
class MqttSource : public AbstractSource<T>
{
public:
    /**
     * @brief MqttSource
     * @param subscriber The Mqtt Topic this source should be subscribed to
     */
    MqttSource(MqttLink::Subscriber& subscriber);

    ~MqttSource() override;


protected:
    /**
     * @brief pre_run Reimplemented from ThreadRunner
     * @return 0 if it should continue to run
     */
    auto pre_run() -> int override;

    /**
     * @brief step implementation from ThreadRunner
     * @return zero if the step succeeded.
     */
    [[nodiscard]] auto step() -> int override;

private:
    /**
    * @brief Adapter base class for the collection of several logically connected, but timely distributed MqttItems
    */
    struct ItemCollector
    {
        ItemCollector();

        /**
        * @brief reset Resets the ItemCollector to its default state
        */
        void reset();

        /**
        * @brief add Tries to add a Message to the Item. The item chooses which messages to keep
        * @param message The message to pass
        * @return 0 if the item is complete with this message, <0 if there was an error, >0 otherwise.
        */
        [[nodiscard]] auto add(MessageParser& topic, MessageParser& message) -> int;

        UserInfo user_info {};
        std::string message_id {};

        std::uint16_t default_status { 0x0000 };
        std::uint16_t status { 0 };

        T item {};
    };

    /**
     * @brief process Processes one LogItem
     * @param msg The message to process
     */
    void process(const MqttLink::Message& msg);

    MqttLink::Subscriber& m_link;

    std::map<std::size_t, ItemCollector> m_buffer {};
};


// +++++++++++++++++++++++++++++++
// implementation part starts here
// +++++++++++++++++++++++++++++++
template <>
MqttSource<DetectorInfo>::ItemCollector::ItemCollector()
    : default_status { 0x00FF }
    , status { default_status }
{
}

template <>
MqttSource<Event>::ItemCollector::ItemCollector()
    : default_status { 0x0001 }
    , status { default_status }
{}

template <typename T>
void MqttSource<T>::ItemCollector::reset() {
    user_info = UserInfo { };
    status = default_status;
}

template <>
auto MqttSource<DetectorInfo>::ItemCollector::add(MessageParser& /*topic*/, MessageParser& message) -> int
{
    if (message_id != message[0]) {
        reset();
        message_id = message[0];
    }
    item.m_hash = user_info.hash();
	item.m_userinfo = user_info;
	
    try {
        if (message[1] == "geoHeightMSL") {
            item.m_location.h = std::stod(message[2], nullptr);
            status &= ~1;
        } else if (message[1] == "geoHorAccuracy") {
            item.m_location.h_acc = std::stod(message[2], nullptr);
            status &= ~2;
        } else if (message[1] == "geoLatitude") {
            item.m_location.lat = std::stod(message[2], nullptr);
            status &= ~4;
        } else if (message[1] == "geoLongitude") {
            item.m_location.lon = std::stod(message[2], nullptr);
            status &= ~8;
        } else if (message[1] == "geoVertAccuracy") {
            item.m_location.v_acc = std::stod(message[2], nullptr);
            status &= ~16;
        } else if (message[1] == "positionDOP") {
            item.m_location.dop = std::stod(message[2], nullptr);
            status &= ~32;
        } else if (message[1] == "timeAccuracy") {
            item.m_time_info.accuracy = std::stod(message[2], nullptr);
            status &= ~64;
        } else if (message[1] == "timeDOP") {
            item.m_time_info.dop = std::stod(message[2], nullptr);
            status &= ~128;
        } else {
            return -1;
        }
    } catch(std::invalid_argument& e) {
        Log::warning()<<"received exception when parsing log item: " + std::string(e.what());
        return -1;
    }

    return status;
}

template <>
auto MqttSource<Event>::ItemCollector::add(MessageParser& topic, MessageParser& content) -> int
{
    if ((topic.size() >= 4) && (content.size() >= 7)) {

        Event::Data data;
        try {
            MessageParser start {content[0], '.'};
            if (start.size() != 2) {
                Log::warning()<<"Message '" + topic.get() + " " + content.get() + "' is invalid.";
                return -1;
            }
            std::int_fast64_t epoch = std::stoll(start[0]) * static_cast<std::int_fast64_t>(1e9);
            data.start = epoch + std::stoll(start[1]) * static_cast<std::int_fast64_t>(std::pow(10, (9 - start[1].length())));

        } catch (...) {
            Log::warning()<<"Message '" + topic.get() + " " + content.get() + "' is invalid.";
            return -1;
        }

        try {
            MessageParser start {content[1], '.'};
            if (start.size() != 2) {
                Log::warning()<<"Message '" + topic.get() + " " + content.get() + "' is invalid.";
                return -1;
            }
            std::int_fast64_t epoch = std::stoll(start[0]) * static_cast<std::int_fast64_t>(1e9);
            data.end = epoch + std::stoll(start[1]) * static_cast<std::int_fast64_t>(std::pow(10, (9 - start[1].length())));
        } catch (...) {
            Log::warning()<<"Message '" + topic.get() + " " + content.get() + "' is invalid.";
            return -1;
        }

        try {
            data.user = topic[2];
            data.station_id = user_info.station_id;
            data.time_acc = static_cast<std::uint32_t>(std::stoul(content[2], nullptr));
            data.ublox_counter = static_cast<std::uint16_t>(std::stoul(content[3], nullptr));
            data.fix = static_cast<std::uint8_t>(std::stoul(content[4], nullptr));
            data.utc = static_cast<std::uint8_t>(std::stoul(content[6], nullptr));
            data.gnss_time_grid = static_cast<std::uint8_t>(std::stoul(content[5], nullptr));
        } catch (std::invalid_argument& e) {
            Log::warning()<<"Received exception: " + std::string(e.what()) + "\n While converting '" + topic.get() + " " + content.get() + "'";
            return -1;
        }
        item = Event{user_info.hash(), data};
        status = 0;
        return 0;
    }
    return -1;
}


template <class T>
MqttSource<T>::MqttSource(MqttLink::Subscriber& subscriber)
    : m_link { subscriber }
{
    AbstractSource<T>::start();
}

template <class T>
MqttSource<T>::~MqttSource() = default;

template <class T>
auto MqttSource<T>::pre_run() -> int
{
    return 0;
}

template <class T>
auto MqttSource<T>::step() -> int
{
    if (m_link.has_message()) {
        MqttLink::Message msg = m_link.get_message();
        this->process(msg);
    }
    std::this_thread::sleep_for(std::chrono::microseconds{50});
    return 0;
}

template <typename T>
void MqttSource<T>::process(const MqttLink::Message& msg)
{
        MessageParser topic { msg.topic, '/'};
        MessageParser content { msg.content, ' '};
        MessageParser subscribe_topic { m_link.get_subscribe_topic(), '/' };


        if ((topic.size() >= 4) && (content.size() >= 2)) {
            if ( (topic[2] == "") || (topic[2] == "cluster") ) {
                return;
            }
            UserInfo userinfo {};
            userinfo.username =  topic[2];
            std::string site { topic[3] };
            for (std::size_t i = 4; i < topic.size(); i++) {
                site += "/" + topic[i];
            }
            userinfo.station_id = site;

            std::size_t hash { userinfo.hash() };

            if ((m_buffer.size() > 0) && (m_buffer.find(hash) != m_buffer.end())) {
                ItemCollector& item { m_buffer[hash] };
                if (item.add(topic, content) == 0) {
                    this->push_item( std::move(item.item) );
                    m_buffer.erase(hash);
                }
            } else {
                ItemCollector item;
                item.message_id = content[0];
                item.user_info = userinfo;
                int value { item.add(topic, content) };
                if (value == 0) {
                    this->push_item( std::move(item.item) );
                } else if (value > 0) {
                    m_buffer.insert( { hash, item } );
                }
            }
        }
}

template <>
void MqttSource<MqttLink::Message>::process(const MqttLink::Message& msg)
{
    push_item( MqttLink::Message { msg } );
}
}

#endif // MQTTLOGSOURCE_H
