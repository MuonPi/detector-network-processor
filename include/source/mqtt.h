#ifndef MQTTLOGSOURCE_H
#define MQTTLOGSOURCE_H

#include "link/mqtt.h"
#include "messages/detectorlog.h"
#include "messages/detectorinfo.h"
#include "messages/event.h"
#include "messages/userinfo.h"
#include "source/base.h"

#include "utility/log.h"
#include "utility/utility.h"

#include <map>
#include <memory>

namespace MuonPi::Source {


/**
 * @brief The Mqtt class
 */
template <typename T>
class Mqtt : public Base<T> {
public:
    /**
     * @brief Mqtt
     * @param subscriber The Mqtt Topic this source should be subscribed to
     */
    Mqtt(Sink::Base<T>& sink, Link::Mqtt::Subscriber& subscriber);

    ~Mqtt() override;

private:
    /**
    * @brief Adapter base class for the collection of several logically connected, but timely distributed MqttItems
    */
    struct ItemCollector {
        enum class ResultCode { Aggregating, Finished, Error, NewEpoch };
		ItemCollector();

        /**
        * @brief reset Resets the ItemCollector to its default state
        */
        void reset();

        /**
        * @brief add Tries to add a Message to the Item. The item chooses which messages to keep
        * @param message The message to pass
        * @return result code
        */
        [[nodiscard]] auto add(MessageParser& topic, MessageParser& message) -> ResultCode;

        UserInfo user_info {};
        std::string message_id {};

        [[nodiscard]] auto is_same_message_id(const std::string& a_message_id) const -> bool { return (a_message_id == message_id); }

		std::uint16_t default_status { 0x0000 };
        std::uint16_t status { 0 };

        T item {};
    };

    /**
     * @brief process Processes one LogItem
     * @param msg The message to process
     */
    void process(const Link::Mqtt::Message& msg);

    Link::Mqtt::Subscriber& m_link;

    std::map<std::size_t, ItemCollector> m_buffer {};
};

// +++++++++++++++++++++++++++++++
// implementation part starts here
// +++++++++++++++++++++++++++++++
template <>
Mqtt<DetectorInfo<Location>>::ItemCollector::ItemCollector()
    : default_status { 0x003F }
    , status { default_status }
{
}

template <>
Mqtt<Event>::ItemCollector::ItemCollector()
    : default_status { 0x0001 }
    , status { default_status }
{
}

template <>
Mqtt<DetectorLog>::ItemCollector::ItemCollector()
    : default_status { 2 }
    , status { default_status }
{
}

template <typename T>
void Mqtt<T>::ItemCollector::reset()
{
    user_info = UserInfo {};
    status = default_status;
}

template <>
auto Mqtt<DetectorInfo<Location>>::ItemCollector::add(MessageParser& /*topic*/, MessageParser& message) -> ResultCode
{
    if (message_id != message[0]) {
        reset();
        message_id = message[0];
    }
    item.m_hash = user_info.hash();
    item.m_userinfo = user_info;
    try {
        if (message[1] == "geoHeightMSL") {
            item.m_item.h = std::stod(message[2], nullptr);
            status &= ~1;
        } else if (message[1] == "geoHorAccuracy") {
            item.m_item.h_acc = std::stod(message[2], nullptr);
            status &= ~2;
        } else if (message[1] == "geoLatitude") {
            item.m_item.lat = std::stod(message[2], nullptr);
            status &= ~4;
        } else if (message[1] == "geoLongitude") {
            item.m_item.lon = std::stod(message[2], nullptr);
            status &= ~8;
        } else if (message[1] == "geoVertAccuracy") {
            item.m_item.v_acc = std::stod(message[2], nullptr);
            status &= ~16;
        } else if (message[1] == "positionDOP") {
            item.m_item.dop = std::stod(message[2], nullptr);
            status &= ~32;
        } else {
            return ResultCode::Aggregating;
        }
    } catch (std::invalid_argument& e) {
        Log::warning() << "received exception when parsing log item: " + std::string(e.what());
        return ResultCode::Error;
    }

    return ((status==0)?ResultCode::Finished:ResultCode::Aggregating);
}

template <>
auto Mqtt<Event>::ItemCollector::add(MessageParser& topic, MessageParser& content) -> ResultCode
{
    if ((topic.size() >= 4) && (content.size() >= 7)) {

        Event::Data data;
        try {
            MessageParser start { content[0], '.' };
            if (start.size() != 2) {
                return ResultCode::Error;
            }
            std::int_fast64_t epoch = std::stoll(start[0]) * static_cast<std::int_fast64_t>(1e9);
            data.start = epoch + std::stoll(start[1]) * static_cast<std::int_fast64_t>(std::pow(10, (9 - start[1].length())));

        } catch (...) {
            return ResultCode::Error;
        }

        try {
            MessageParser start { content[1], '.' };
            if (start.size() != 2) {
                return ResultCode::Error;
            }
            std::int_fast64_t epoch = std::stoll(start[0]) * static_cast<std::int_fast64_t>(1e9);
            data.end = epoch + std::stoll(start[1]) * static_cast<std::int_fast64_t>(std::pow(10, (9 - start[1].length())));
        } catch (...) {
            return ResultCode::Error;
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
            Log::warning() << "Received exception: " + std::string(e.what()) + "\n While converting '" + topic.get() + " " + content.get() + "'";
            return ResultCode::Error;
        }
        item = Event { user_info.hash(), data };
        status = 0;
        return ResultCode::Finished;
    }
    return ResultCode::Error;
}

template <>
auto Mqtt<DetectorLog>::ItemCollector::add(MessageParser& /*topic*/, MessageParser& message) -> ResultCode
{
    if ( !item.has_items() ) {
		message_id = message[0];
		item.set_log_id( message_id );
		item.set_userinfo( user_info );
	}
	else if (message_id != message[0]) {
        return ResultCode::NewEpoch;
    }

    try {
        if (message[1] == "geoHeightMSL") {
            item.add_item( { "geoHeightMSL", std::stod(message[2], nullptr) } );
        } else if (message[1] == "geoHorAccuracy") {
            item.add_item( { "geoHorAccuracy", std::stod(message[2], nullptr) } );
        } else if (message[1] == "geoLatitude") {
            item.add_item( { "geoLatitude", std::stod(message[2], nullptr) } );
        } else if (message[1] == "geoLongitude") {
            item.add_item( { "geoLongitude", std::stod(message[2], nullptr) } );
        } else if (message[1] == "geoVertAccuracy") {
            item.add_item( { "geoVertAccuracy", std::stod(message[2], nullptr) } );
        } else if (message[1] == "positionDOP") {
            item.add_item( { "positionDOP", std::stod(message[2], nullptr) } );
        } else {
            return ResultCode::Aggregating;
        }
    } catch (std::invalid_argument& e) {
        Log::warning() << "received exception when parsing log item: " + std::string(e.what());
        return ResultCode::Error;
    }

    return ResultCode::Aggregating;
}



template <typename T>
Mqtt<T>::Mqtt(Sink::Base<T>& sink, Link::Mqtt::Subscriber& subscriber)
    : Base<T> { sink }
    , m_link { subscriber }
{
    subscriber.set_callback([this](const Link::Mqtt::Message& message) {
        process(message);
    });
}

template <typename T>
Mqtt<T>::~Mqtt() = default;

template <typename T>
void Mqtt<T>::process(const Link::Mqtt::Message& msg)
{
    MessageParser topic { msg.topic, '/' };
    MessageParser content { msg.content, ' ' };
    MessageParser subscribe_topic { m_link.get_subscribe_topic(), '/' };

    if ((topic.size() >= 4) && (content.size() >= 2)) {
        if ((topic[2] == "") || (topic[2] == "cluster")) {
            return;
        }
        UserInfo userinfo {};
        userinfo.username = topic[2];
        std::string site { topic[3] };
        for (std::size_t i = 4; i < topic.size(); i++) {
            site += "/" + topic[i];
        }
        userinfo.station_id = site;

        std::size_t hash { userinfo.hash() };

        if ((m_buffer.size() > 0) && (m_buffer.find(hash) != m_buffer.end())) {
            ItemCollector& item { m_buffer[hash] };
            typename ItemCollector::ResultCode result_code { item.add(topic, content) };
			if ( result_code == ItemCollector::ResultCode::Finished ) {
                this->put(std::move(item.item));
                m_buffer.erase(hash);
            } else if (result_code == ItemCollector::ResultCode::NewEpoch) {
				// the new message has a newer log id
				// push the item out and create a new item with the last message
				this->put(std::move(item.item));
				item = ItemCollector { };
				item.message_id = content[0];
				item.user_info = userinfo;
				typename ItemCollector::ResultCode retry_result_code { item.add(topic, content) };
				if ( retry_result_code == ItemCollector::ResultCode::Finished ) {
					this->put(std::move(item.item));
				} else if (retry_result_code == ItemCollector::ResultCode::Aggregating) {
					m_buffer.insert({ hash, item });
				}
			}
        } else {
            ItemCollector item;
            item.message_id = content[0];
            item.user_info = userinfo;
            typename ItemCollector::ResultCode value { item.add(topic, content) };
            if ( value == ItemCollector::ResultCode::Finished ) {
                this->put(std::move(item.item));
            } else if (value == ItemCollector::ResultCode::Aggregating) {
                m_buffer.insert({ hash, item });
            } 
        }
    }
}

template <>
void Mqtt<Link::Mqtt::Message>::process(const Link::Mqtt::Message& msg)
{
    put(Link::Mqtt::Message { msg });
}
}

#endif // MQTTLOGSOURCE_H
