#ifndef MQTTLOGSOURCE_H
#define MQTTLOGSOURCE_H

#include "messages/detectorinfo.h"
#include "messages/detectorlog.h"
#include "messages/event.h"
#include "messages/userinfo.h"
#include <muonpi/link/mqtt.h>

#include <muonpi/source/base.h>

#include <muonpi/log.h>
#include <muonpi/utility.h>

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
    struct configuration {
        int max_geohash_length {};
    };
    /**
     * @brief mqtt
     * @param subscriber The mqtt Topic this source should be subscribed to
     */
    mqtt(sink::base<T>& sink, link::mqtt::subscriber& topic, configuration config);

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

        configuration m_config {};
    };

    /**
     * @brief process Processes one LogItem
     * @param msg The message to process
     */
    void process(const link::mqtt::message_t& msg);

    [[nodiscard]] auto generate_hash(message_parser& topic, message_parser& message) -> std::size_t;

    link::mqtt::subscriber& m_link;

    std::map<std::size_t, item_collector> m_buffer {};

    configuration m_config {};
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
        log::warning() << "received exception when parsing log item: " << e.what();
        return ResultCode::Error;
    }

    if (item.item<location_t>().max_geohash_length == 0) {
        item.item<location_t>().max_geohash_length = m_config.max_geohash_length;
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
            log::warning() << "Received exception: " << e.what() << "\n While converting '" << topic.get() << " " << content.get() << "'";
            return Error;
        }
        if (status == 0) {

            item = event_t { data };
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
        log::warning() << "Received exception: " << e.what() << "\n While converting '" << topic.get() << " " << content.get() << "'";
        return Error;
    } catch (...) {
        return Error;
    }
    if (data.start > data.end) {
        return Error;
    }
    item = event_t { data };
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
    // clang-format off
    static const std::map<std::string, detector_log_t::item::Type> mapping {
          {"UBX_HW_Version"       , detector_log_t::item::Type::String}
        , {"UBX_Prot_Version"     , detector_log_t::item::Type::String}
        , {"UBX_SW_Version"       , detector_log_t::item::Type::String}
        , {"hardwareVersionString", detector_log_t::item::Type::String}
        , {"softwareVersionString", detector_log_t::item::Type::String}
        , {"maxGeohashLength"     , detector_log_t::item::Type::String}
        , {"uniqueId"             , detector_log_t::item::Type::String}
        , {"geoHash"              , detector_log_t::item::Type::String}

        , {"gainSwitch"           , detector_log_t::item::Type::Int}
        , {"polaritySwitch1"      , detector_log_t::item::Type::Int}
        , {"polaritySwitch2"      , detector_log_t::item::Type::Int}
        , {"preampSwitch1"        , detector_log_t::item::Type::Int}
        , {"preampSwitch2"        , detector_log_t::item::Type::Int}
        , {"systemNrCPUs"         , detector_log_t::item::Type::Int}

        , {"geoHeightMSL"         , detector_log_t::item::Type::Double}
        , {"geoHorAccuracy"       , detector_log_t::item::Type::Double}
        , {"geoLatitude"          , detector_log_t::item::Type::Double}
        , {"geoLongitude"         , detector_log_t::item::Type::Double}
        , {"geoVertAccuracy"      , detector_log_t::item::Type::Double}
        , {"positionDOP"          , detector_log_t::item::Type::Double}
        , {"RXBufUsage"           , detector_log_t::item::Type::Double}
        , {"TXBufUsage"           , detector_log_t::item::Type::Double}
        , {"adcSamplingTime"      , detector_log_t::item::Type::Double}
        , {"antennaPower"         , detector_log_t::item::Type::Double}
        , {"antennaStatus"        , detector_log_t::item::Type::Double}
        , {"biasDAC"              , detector_log_t::item::Type::Double}
        , {"biasSwitch"           , detector_log_t::item::Type::Double}
        , {"calib_coeff2"         , detector_log_t::item::Type::Double}
        , {"calib_coeff3"         , detector_log_t::item::Type::Double}
        , {"calib_rsense"         , detector_log_t::item::Type::Double}
        , {"calib_vdiv"           , detector_log_t::item::Type::Double}
        , {"clockBias"            , detector_log_t::item::Type::Double}
        , {"clockDrift"           , detector_log_t::item::Type::Double}
        , {"fixStatus"            , detector_log_t::item::Type::Double}
        , {"freqAccuracy"         , detector_log_t::item::Type::Double}
        , {"ibias"                , detector_log_t::item::Type::Double}
        , {"jammingLevel"         , detector_log_t::item::Type::Double}
        , {"maxCNR"               , detector_log_t::item::Type::Double}
        , {"maxRXBufUsage"        , detector_log_t::item::Type::Double}
        , {"meanGeoHeightMSL"     , detector_log_t::item::Type::Double}
        , {"preampAGC"            , detector_log_t::item::Type::Double}
        , {"preampNoise"          , detector_log_t::item::Type::Double}
        , {"rateAND"              , detector_log_t::item::Type::Double}
        , {"rateXOR"              , detector_log_t::item::Type::Double}
        , {"sats"                 , detector_log_t::item::Type::Double}
        , {"systemFreeMem"        , detector_log_t::item::Type::Double}
        , {"systemFreeSwap"       , detector_log_t::item::Type::Double}
        , {"systemLoadAvg"        , detector_log_t::item::Type::Double}
        , {"systemUptime"         , detector_log_t::item::Type::Double}
        , {"temperature"          , detector_log_t::item::Type::Double}
        , {"thresh1"              , detector_log_t::item::Type::Double}
        , {"thresh2"              , detector_log_t::item::Type::Double}
        , {"timeAccuracy"         , detector_log_t::item::Type::Double}
        , {"timeDOP"              , detector_log_t::item::Type::Double}
        , {"ubloxUptime"          , detector_log_t::item::Type::Double}
        , {"usedSats"             , detector_log_t::item::Type::Double}
        , {"vbias"                , detector_log_t::item::Type::Double}
        , {"vsense"               , detector_log_t::item::Type::Double}
    };
    // clang-format on

    detector_log_t::item::Type type { detector_log_t::item::Type::String };

    if (mapping.count(message[1]) > 0) {
        type = mapping.at(message[1]);
    }

    std::string unit {};
    if (message.size() > 3) {
        unit = message[3];
    }

    try {
        if (type == detector_log_t::item::Type::Int) {
            item.emplace({ message[1], std::stoi(message[2], nullptr, 10), unit });
        } else if (type == detector_log_t::item::Type::Double) {
            item.emplace({ message[1], std::stod(message[2], nullptr), unit });
        } else {
            item.emplace({ message[1], message[2], unit });
        }
    } catch (std::invalid_argument& e) {
        log::warning() << "received exception when parsing log item: " << e.what();
        return Error;
    }

    return Aggregating;
}

template <typename T>
mqtt<T>::mqtt(sink::base<T>& sink, link::mqtt::subscriber& topic, configuration config)
    : base<T> { sink }
    , m_link { topic }
    , m_config { std::move(config) }
{
    topic.emplace_callback([this](const link::mqtt::message_t& message) {
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
    item.m_config = m_config;
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
