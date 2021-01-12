#ifndef MQTTLOGSOURCE_H
#define MQTTLOGSOURCE_H

#include "link/mqtt.h"
#include "messages/detectorinfo.h"
#include "messages/detectorlog.h"
#include "messages/event.h"
#include "messages/userinfo.h"
#include "source/base.h"

#include "utility/log.h"
#include "utility/utility.h"

#include <map>
#include <memory>
#include <string>

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
    Mqtt(Sink::Base<T>& sink, Link::Mqtt::Subscriber& topic);

    ~Mqtt() override;

private:
    /**
    * @brief Adapter base class for the collection of several logically connected, but timely distributed MqttItems
    */
    struct ItemCollector {
        enum ResultCode : std::uint8_t {
            Error = 0,
            Aggregating = 1,
            Finished = 2,
            Abort = 4,
            NewEpoch = 8,
            Commit = Finished | NewEpoch,
            Reset = Abort | NewEpoch
        };

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

    [[nodiscard]] auto generate_hash(MessageParser& topic, MessageParser& message) -> std::size_t;

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
    : default_status { 0x0000 }
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
        return Reset;
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

    return ((status == 0) ? ResultCode::Finished : ResultCode::Aggregating);
}

template <>
auto Mqtt<Event>::ItemCollector::add(MessageParser& topic, MessageParser& content) -> ResultCode
{
    if ((topic.size() < 4) || (content.size() < 7)) {
        return Error;
    }
    if (topic[1] == "l1data") {
        if (content.size() < 13) {
            return Error;
        }

        Event::Data data;

        std::size_t hash { 0 };
        std::size_t n { 0 };
        try {
            hash = std::stoul(content[1], nullptr, 16);
            n = std::stoul(content[4], nullptr);
            data.user = topic[2];
            data.station_id = topic[3];
            data.time_acc = static_cast<std::uint32_t>(std::stoul(content[3], nullptr));
            data.ublox_counter = static_cast<std::uint16_t>(std::stoul(content[7], nullptr));
            data.fix = static_cast<std::uint8_t>(std::stoul(content[10], nullptr));
            data.utc = static_cast<std::uint8_t>(std::stoul(content[12], nullptr));
            data.gnss_time_grid = static_cast<std::uint8_t>(std::stoul(content[9], nullptr));
            data.start = std::stoll(content[11], nullptr);
            data.end = std::stoll(content[8], nullptr) + data.start;
        } catch (std::invalid_argument& e) {
            Log::warning() << "Received exception: " + std::string(e.what()) + "\n While converting '" + topic.get() + " " + content.get() + "'";
            return Error;
        }
        if (status == 0) {
            item = Event { hash, data };
            status = n - 1;
            return Aggregating;
        } else {
            item.add_event(Event { hash, data });
            status--;
            if (status == 0) {
                return Finished;
            }
        }
    }
    Event::Data data;
    try {
        MessageParser start { content[0], '.' };
        if (start.size() != 2) {
            return Error;
        }
        std::int_fast64_t epoch = std::stoll(start[0]) * static_cast<std::int_fast64_t>(1e9);
        data.start = epoch + std::stoll(start[1]) * static_cast<std::int_fast64_t>(std::pow(10, (9 - start[1].length())));

    } catch (...) {
        return Error;
    }

    try {
        MessageParser start { content[1], '.' };
        if (start.size() != 2) {
            return ResultCode::Error;
        }
        std::int_fast64_t epoch = std::stoll(start[0]) * static_cast<std::int_fast64_t>(1e9);
        data.end = epoch + std::stoll(start[1]) * static_cast<std::int_fast64_t>(std::pow(10, (9 - start[1].length())));
    } catch (...) {
        return Error;
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
        return Error;
    }
    item = Event { user_info.hash(), data };
    status = 0;
    return Finished;
}

template <>
auto Mqtt<DetectorLog>::ItemCollector::add(MessageParser& /*topic*/, MessageParser& message) -> ResultCode
{
    if (!item.has_items()) {
        message_id = message[0];
        item.set_log_id(message_id);
        item.set_userinfo(user_info);
    } else if (message_id != message[0]) {
        return Commit;
    }

    std::string unit {};
    if (message.size() > 3) {
        unit = message[3];
    }
    try {
        if (message[1] == "geoHeightMSL") {
            item.add_item({ "geoHeightMSL", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "geoHorAccuracy") {
            item.add_item({ "geoHorAccuracy", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "geoLatitude") {
            item.add_item({ "geoLatitude", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "geoLongitude") {
            item.add_item({ "geoLongitude", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "geoVertAccuracy") {
            item.add_item({ "geoVertAccuracy", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "positionDOP") {
            item.add_item({ "positionDOP", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "RXBufUsage") {
            item.add_item({ "RXBufUsage", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "TXBufUsage") {
            item.add_item({ "TXBufUsage", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "UBX_HW_Version") {
            item.add_item({ "UBX_HW_Version", message[2], "" });
        } else if (message[1] == "UBX_Prot_Version") {
            item.add_item({ "UBX_Prot_Version", message[2], "" });
        } else if (message[1] == "UBX_SW_Version") {
            item.add_item({ "UBX_SW_Version", message[2], "" });
        } else if (message[1] == "adcSamplingTime") {
            item.add_item({ "adcSamplingTime", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "antennaPower") {
            item.add_item({ "antennaPower", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "antennaStatus") {
            item.add_item({ "antennaStatus", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "biasDAC") {
            item.add_item({ "biasDAC", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "biasSwitch") {
            item.add_item({ "biasSwitch", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "calib_coeff2") {
            item.add_item({ "calib_coeff2", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "calib_coeff3") {
            item.add_item({ "calib_coeff3", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "calib_rsense") {
            item.add_item({ "calib_rsense", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "calib_vdiv") {
            item.add_item({ "calib_vdiv", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "clockBias") {
            item.add_item({ "clockBias", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "clockDrift") {
            item.add_item({ "clockDrift", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "fixStatus") {
            item.add_item({ "fixStatus", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "freqAccuracy") {
            item.add_item({ "freqAccuracy", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "gainSwitch") {
            item.add_item({ "gainSwitch", static_cast<std::uint8_t>(std::stoi(message[2], nullptr, 10)), unit });
        } else if (message[1] == "ibias") {
            item.add_item({ "ibias", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "jammingLevel") {
            item.add_item({ "jammingLevel", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "maxCNR") {
            item.add_item({ "maxCNR", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "maxRXBufUsage") {
            item.add_item({ "maxTXBufUsage", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "meanGeoHeightMSL") {
            item.add_item({ "meanGeoHeightMSL", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "polaritySwitch1") {
            item.add_item({ "polaritySwitch1", static_cast<std::uint8_t>(std::stoi(message[2], nullptr, 10)), unit });
        } else if (message[1] == "polaritySwitch2") {
            item.add_item({ "polaritySwitch2", static_cast<std::uint8_t>(std::stoi(message[2], nullptr, 10)), unit });
        } else if (message[1] == "preampAGC") {
            item.add_item({ "preampAGC", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "preampNoise") {
            item.add_item({ "preampNoise", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "preampSwitch1") {
            item.add_item({ "preampSwitch1", static_cast<std::uint8_t>(std::stoi(message[2], nullptr, 10)), unit });
        } else if (message[1] == "preampSwitch2") {
            item.add_item({ "preampSwitch2", static_cast<std::uint8_t>(std::stoi(message[2], nullptr, 10)), unit });
        } else if (message[1] == "rateAND") {
            item.add_item({ "rateAND", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "rateXOR") {
            item.add_item({ "rateXOR", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "sats") {
            item.add_item({ "sats", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "systemFreeMem") {
            item.add_item({ "systemFreeMem", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "systemFreeSwap") {
            item.add_item({ "systemFreeSwap", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "systemLoadAvg") {
            item.add_item({ "systemLoadAvg", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "systemNrCPUs") {
            item.add_item({ "systemNrCPUs", static_cast<std::uint16_t>(std::stoi(message[2], nullptr, 10)), unit });
        } else if (message[1] == "systemUptime") {
            item.add_item({ "systemUptime", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "temperature") {
            item.add_item({ "temperature", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "thresh1") {
            item.add_item({ "thresh1", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "thresh2") {
            item.add_item({ "thresh2", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "timeAccuracy") {
            item.add_item({ "timeAccuracy", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "timeDOP") {
            item.add_item({ "timeDOP", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "ubloxUptime") {
            item.add_item({ "ubloxUptime", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "usedSats") {
            item.add_item({ "usedSats", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "vbias") {
            item.add_item({ "vbias", std::stod(message[2], nullptr), unit });
        } else if (message[1] == "vsense") {
            item.add_item({ "vsense", std::stod(message[2], nullptr), unit });
        } else {
            return Aggregating;
        }
    } catch (std::invalid_argument& e) {
        Log::warning() << "received exception when parsing log item: " + std::string(e.what());
        return Error;
    }

    return Aggregating;
}

template <typename T>
Mqtt<T>::Mqtt(Sink::Base<T>& sink, Link::Mqtt::Subscriber& topic)
    : Base<T> { sink }
    , m_link { topic }
{
    topic.set_callback([this](const Link::Mqtt::Message& message) {
        process(message);
    });
}

template <typename T>
Mqtt<T>::~Mqtt() = default;

template <typename T>
auto Mqtt<T>::generate_hash(MessageParser& topic, MessageParser& /*message*/) -> std::size_t
{
    UserInfo userinfo {};
    userinfo.username = topic[2];
    std::string site { topic[3] };
    for (std::size_t i = 4; i < topic.size(); i++) {
        site += "/" + topic[i];
    }
    userinfo.station_id = site;

    return userinfo.hash();
}

template <>
auto Mqtt<Event>::generate_hash(MessageParser& /*topic*/, MessageParser& message) -> std::size_t
{
    return std::hash<std::string> {}(message[0]);
}

template <typename T>
void Mqtt<T>::process(const Link::Mqtt::Message& msg)
{
    MessageParser topic { msg.topic, '/' };
    MessageParser content { msg.content, ' ' };

    if ((topic.size() < 4) || (content.size() < 2)) {
        return;
    }
    if ((topic[2] == "") || (topic[2] == "cluster")) {
        return;
    }

    std::size_t hash { generate_hash(topic, content) };

    if ((m_buffer.size() > 0) && (m_buffer.find(hash) != m_buffer.end())) {
        ItemCollector& item { m_buffer[hash] };
        auto result_code { item.add(topic, content) };
        if ((result_code & ItemCollector::Finished) != 0) {
            this->put(std::move(item.item));
            m_buffer.erase(hash);
        } else if ((result_code & ItemCollector::Abort) != 0) {
            m_buffer.erase(hash);
        } else {
            return;
        }
        if ((result_code & ItemCollector::NewEpoch) == 0) {
            return;
        }
    }

    UserInfo userinfo {};
    userinfo.username = topic[2];
    std::string site { topic[3] };
    for (std::size_t i = 4; i < topic.size(); i++) {
        site += "/" + topic[i];
    }
    userinfo.station_id = site;

    ItemCollector item;
    item.message_id = content[0];
    item.user_info = userinfo;
    auto value { item.add(topic, content) };
    if ((value & ItemCollector::Finished) != 0) {
        this->put(std::move(item.item));
    } else if ((value & ItemCollector::Aggregating) != 0) {
        m_buffer.insert({ hash, item });
    }
}

template <>
void Mqtt<Link::Mqtt::Message>::process(const Link::Mqtt::Message& msg)
{
    put(Link::Mqtt::Message { msg });
}
}

#endif // MQTTLOGSOURCE_H
