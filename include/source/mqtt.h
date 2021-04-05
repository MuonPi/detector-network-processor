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

#include <algorithm>
#include <map>
#include <memory>
#include <string>

namespace muonpi::source {

/**
 * @brief The source::mqtt class
 */
template <typename T>
class mqtt : public base<T> {
public:
    /**
     * @brief mqtt
     * @param subscriber The mqtt Topic this source should be subscribed to
     */
    mqtt(sink::base<T>& sink, link::mqtt::subscriber& topic);

    ~mqtt() override;

private:
    /**
    * @brief Adapter base class for the collection of several logically connected, but timely distributed mqttItems
    */
    struct item_collector {
        enum ResultCode : std::uint8_t {
            Error = 0,
            Aggregating = 1,
            Finished = 2,
            Abort = 4,
            NewEpoch = 8,
            Commit = Finished | NewEpoch,
            Reset = Abort | NewEpoch
        };

        item_collector();

        /**
        * @brief reset Resets the item_collector to its default state
        */
        void reset();

        /**
        * @brief add Tries to add a Message to the Item. The item chooses which messages to keep
        * @param message The message to pass
        * @return result code
        */
        [[nodiscard]] auto add(message_parser& topic, message_parser& message) -> ResultCode;

        userinfo_t user_info {};

        const std::chrono::system_clock::time_point m_first_message { std::chrono::system_clock::now() };

        std::uint16_t default_status { 0x0000 };
        std::uint16_t status { 0 };

        T item {};
    };

    /**
     * @brief process Processes one LogItem
     * @param msg The message to process
     */
    void process(const link::mqtt::message_t& msg);

    [[nodiscard]] auto generate_hash(message_parser& topic, message_parser& message) -> std::size_t;

    link::mqtt::subscriber& m_link;

    std::map<std::size_t, item_collector> m_buffer {};
};

// +++++++++++++++++++++++++++++++
// implementation part starts here
// +++++++++++++++++++++++++++++++
template <>
mqtt<detector_info_t<location_t>>::item_collector::item_collector()
    : default_status { 0x003F }
    , status { default_status }
{
}

template <>
mqtt<event_t>::item_collector::item_collector()
    : default_status { 0x0000 }
    , status { default_status }
{
}

template <>
mqtt<detector_log_t>::item_collector::item_collector()
    : default_status { 2 }
    , status { default_status }
{
}

template <typename T>
void mqtt<T>::item_collector::reset()
{
    user_info = userinfo_t {};
    status = default_status;
}

template <>
auto mqtt<detector_info_t<location_t>>::item_collector::add(message_parser& /*topic*/, message_parser& message) -> ResultCode
{
    if ((std::chrono::system_clock::now() - m_first_message) > std::chrono::seconds { 5 }) {
        return Reset;
    }
    item.hash = user_info.hash();
    item.userinfo = user_info;
    try {
        if (message[1] == "geoHeightMSL") {
            item.item<location_t>().h = std::stod(message[2], nullptr);
            status &= ~1;
        } else if (message[1] == "geoHorAccuracy") {
            item.item<location_t>().h_acc = std::stod(message[2], nullptr);
            status &= ~2;
        } else if (message[1] == "geoLatitude") {
            item.item<location_t>().lat = std::stod(message[2], nullptr);
            status &= ~4;
        } else if (message[1] == "geoLongitude") {
            item.item<location_t>().lon = std::stod(message[2], nullptr);
            status &= ~8;
        } else if (message[1] == "geoVertAccuracy") {
            item.item<location_t>().v_acc = std::stod(message[2], nullptr);
            status &= ~16;
        } else if (message[1] == "positionDOP") {
            item.item<location_t>().dop = std::stod(message[2], nullptr);
            status &= ~32;
        } else if (message[1] == "maxGeohashLength") {
            item.item<location_t>().max_geohash_length = std::stoi(message[2], nullptr);
        } else {
            return ResultCode::Aggregating;
        }
    } catch (std::invalid_argument& e) {
        log::warning() << "received exception when parsing log item: " + std::string(e.what());
        return ResultCode::Error;
    }

    if (item.item<location_t>().max_geohash_length == 0) {
        item.item<location_t>().max_geohash_length = Config::meta.max_geohash_length;
    }
    return ((status == 0) ? ResultCode::Finished : ResultCode::Aggregating);
}

template <>
auto mqtt<event_t>::item_collector::add(message_parser& topic, message_parser& content) -> ResultCode
{
    if ((topic.size() < 4) || (content.size() < 7)) {
        return Error;
    }
    if (topic[1] == "l1data") {
        if (content.size() < 13) {
            return Error;
        }

        event_t::data_t data;

        std::size_t n { 0 };
        try {
            data.hash = std::stoul(content[1], nullptr, 16);
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
            log::warning() << "Received exception: " + std::string(e.what()) + "\n While converting '" + topic.get() + " " + content.get() + "'";
            return Error;
        }
        if (status == 0) {

            item = event_t{data};
            status = n - 1;
            return Aggregating;
        }
        item.emplace(data);
        status--;
        if (status == 0) {
            return Finished;
        } else {
            return Aggregating;
        }
    }

    event_t::data_t data;

    try {

        if ((content[0].length() < 17) || (content[1].length() < 17)) {
            return Error;
        }

        if ((content[0][0] == '.') || (content[1][0] == '.')) {
            return Error;
        }

        data.hash = user_info.hash();
        data.start = static_cast<std::int_fast64_t>(std::stold(content[0]) * 1e9);
        data.end = static_cast<std::int_fast64_t>(std::stold(content[1]) * 1e9);
        data.user = topic[2];
        data.station_id = user_info.station_id;
        data.time_acc = static_cast<std::uint32_t>(std::stoul(content[2], nullptr));
        data.ublox_counter = static_cast<std::uint16_t>(std::stoul(content[3], nullptr));
        data.fix = static_cast<std::uint8_t>(std::stoul(content[4], nullptr));
        data.utc = static_cast<std::uint8_t>(std::stoul(content[6], nullptr));
        data.gnss_time_grid = static_cast<std::uint8_t>(std::stoul(content[5], nullptr));
    } catch (std::invalid_argument& e) {
        log::warning() << "Received exception: " + std::string(e.what()) + "\n While converting '" + topic.get() + " " + content.get() + "'";
        return Error;
    } catch (...) {
        return Error;
    }
    if (data.start > data.end) {
        return Error;
    }
    item = event_t{data};
    status = 0;
    return Finished;
}

template <>
auto mqtt<detector_log_t>::item_collector::add(message_parser& /*topic*/, message_parser& message) -> ResultCode
{
    if (item.items.empty()) {
        item.log_id = message[0];
        item.userinfo = user_info;
    } else if ((std::chrono::system_clock::now() - m_first_message) > std::chrono::seconds { 5 }) {
        return Commit;
    }

    std::string unit {};
    if (message.size() > 3) {
        unit = message[3];
    }
    try {
        if ((message[1] == "geoHeightMSL")
            || (message[1] == "geoHorAccuracy")
            || (message[1] == "geoLatitude")
            || (message[1] == "geoLongitude")
            || (message[1] == "geoVertAccuracy")
            || (message[1] == "positionDOP")
            || (message[1] == "RXBufUsage")
            || (message[1] == "TXBufUsage")
            || (message[1] == "adcSamplingTime")
            || (message[1] == "antennaPower")
            || (message[1] == "antennaStatus")
            || (message[1] == "biasDAC")
            || (message[1] == "biasSwitch")
            || (message[1] == "calib_coeff2")
            || (message[1] == "calib_coeff3")
            || (message[1] == "calib_rsense")
            || (message[1] == "calib_vdiv")
            || (message[1] == "clockBias")
            || (message[1] == "clockDrift")
            || (message[1] == "fixStatus")
            || (message[1] == "freqAccuracy")
            || (message[1] == "ibias")
            || (message[1] == "jammingLevel")
            || (message[1] == "maxCNR")
            || (message[1] == "maxRXBufUsage")
            || (message[1] == "meanGeoHeightMSL")
            || (message[1] == "preampAGC")
            || (message[1] == "preampNoise")
            || (message[1] == "rateAND")
            || (message[1] == "rateXOR")
            || (message[1] == "sats")
            || (message[1] == "systemFreeMem")
            || (message[1] == "systemFreeSwap")
            || (message[1] == "systemLoadAvg")
            || (message[1] == "systemUptime")
            || (message[1] == "temperature")
            || (message[1] == "thresh1")
            || (message[1] == "thresh2")
            || (message[1] == "timeAccuracy")
            || (message[1] == "timeDOP")
            || (message[1] == "ubloxUptime")
            || (message[1] == "usedSats")
            || (message[1] == "vbias")
            || (message[1] == "vsense")) {
            item.emplace({ message[1], std::stod(message[2], nullptr), unit });
        } else if (
            (message[1] == "UBX_HW_Version")
            || (message[1] == "UBX_Prot_Version")
            || (message[1] == "UBX_SW_Version")
            || (message[1] == "geoHash")) {
            item.emplace({ message[1], message[2], "" });
        } else if (
            (message[1] == "gainSwitch")
            || (message[1] == "polaritySwitch1")
            || (message[1] == "polaritySwitch2")
            || (message[1] == "preampSwitch1")
            || (message[1] == "preampSwitch2")) {
            item.emplace({ message[1], static_cast<std::uint8_t>(std::stoi(message[2], nullptr, 10)), unit });
        } else if (message[1] == "systemNrCPUs") {
            item.emplace({ message[1], static_cast<std::uint16_t>(std::stoi(message[2], nullptr, 10)), unit });
        } else {
            // unknown log message, forward as string as it is
            item.emplace({ message[1], message.get(), "" });
        }
    } catch (std::invalid_argument& e) {
        log::warning() << "received exception when parsing log item: " + std::string(e.what());
        return Error;
    }

    return Aggregating;
}

template <typename T>
mqtt<T>::mqtt(sink::base<T>& sink, link::mqtt::subscriber& topic)
    : base<T> { sink }
    , m_link { topic }
{
    topic.set_callback([this](const link::mqtt::message_t& message) {
        process(message);
    });
}

template <typename T>
mqtt<T>::~mqtt() = default;

template <typename T>
auto mqtt<T>::generate_hash(message_parser& topic, message_parser& /*message*/) -> std::size_t
{
    userinfo_t userinfo {};
    userinfo.username = topic[2];
    std::string site { topic[3] };
    for (std::size_t i = 4; i < topic.size(); i++) {
        site += "/" + topic[i];
    }
    userinfo.station_id = site;

    return userinfo.hash();
}

template <>
auto mqtt<event_t>::generate_hash(message_parser& /*topic*/, message_parser& message) -> std::size_t
{
    return std::hash<std::string> {}(message[0]);
}

template <typename T>
void mqtt<T>::process(const link::mqtt::message_t& msg)
{
    message_parser topic { msg.topic, '/' };
    message_parser content { msg.content, ' ' };

    if ((topic.size() < 4) || (content.size() < 2)) {
        return;
    }
    if ((topic[2] == "") || (topic[2] == "cluster")) {
        return;
    }

    std::size_t hash { generate_hash(topic, content) };

    if ((m_buffer.size() > 0) && (m_buffer.find(hash) != m_buffer.end())) {
        item_collector& item { m_buffer[hash] };
        auto result_code { item.add(topic, content) };
        if ((result_code & item_collector::Finished) != 0) {
            this->put(std::move(item.item));
            m_buffer.erase(hash);
        } else if ((result_code & item_collector::Abort) != 0) {
            m_buffer.erase(hash);
        } else {
            return;
        }
        if ((result_code & item_collector::NewEpoch) == 0) {
            return;
        }
    }

    userinfo_t userinfo {};
    userinfo.username = topic[2];
    std::string site { topic[3] };
    for (std::size_t i = 4; i < topic.size(); i++) {
        site += "/" + topic[i];
    }
    userinfo.station_id = site;

    item_collector item;
    item.user_info = userinfo;
    auto value { item.add(topic, content) };
    if ((value & item_collector::Finished) != 0) {
        this->put(std::move(item.item));
    } else if ((value & item_collector::Aggregating) != 0) {
        m_buffer.insert({ hash, item });
    }
}

template <>
void mqtt<link::mqtt::message_t>::process(const link::mqtt::message_t& msg)
{
    put(link::mqtt::message_t { msg });
}
}

#endif // MQTTLOGSOURCE_H
