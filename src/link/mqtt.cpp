#include "link/mqtt.h"
#include "utility/exceptions.h"
#include "utility/log.h"

#include <cstring>
#include <functional>
#include <regex>
#include <sstream>
#include <utility>

#include <cryptopp/filters.h>
#include <cryptopp/hex.h>
#include <cryptopp/sha.h>

namespace muonpi::link {

auto mqtt::wait_for(Status status, std::chrono::milliseconds duration) -> bool
{
    std::chrono::steady_clock::time_point start { std::chrono::steady_clock::now() };
    while (((std::chrono::steady_clock::now() - start) < duration)) {
        if (m_status == status) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds { 10 });
    }
    return false;
}

void wrapper_callback_connected(mosquitto* /*mqtt*/, void* object, int result)
{
    reinterpret_cast<mqtt*>(object)->callback_connected(result);
}

void wrapper_callback_disconnected(mosquitto* /*mqtt*/, void* object, int result)
{
    reinterpret_cast<mqtt*>(object)->callback_disconnected(result);
}

void wrapper_callback_message(mosquitto* /*mqtt*/, void* object, const mosquitto_message* message)
{
    reinterpret_cast<mqtt*>(object)->callback_message(message);
}

mqtt::mqtt(Config::Mqtt config)
    : thread_runner { "mqtt" }
    , m_config { std::move(config) }
    , m_mqtt { init(client_id().c_str()) }
{
    mosquitto_connect_callback_set(m_mqtt, wrapper_callback_connected);
    mosquitto_disconnect_callback_set(m_mqtt, wrapper_callback_disconnected);
    mosquitto_message_callback_set(m_mqtt, wrapper_callback_message);

    start();
}

mqtt::mqtt()
    : thread_runner("mqtt")
    , m_mqtt { init(client_id().c_str()) }
{
}

mqtt::~mqtt()
    = default;

auto mqtt::pre_run() -> int
{
    if (!connect()) {
        return -1;
    }
    if ((m_status != Status::Connected) && (m_status != Status::Connecting)) {
        return -1;
    }
    return 0;
}

auto mqtt::step() -> int
{
    if ((m_status != Status::Connected) && (m_status != Status::Connecting)) {
        if (!reconnect()) {
            return -1;
        }
    }
    auto status = mosquitto_loop(m_mqtt, 100, 1);
    if (status != MOSQ_ERR_SUCCESS) {
        switch (status) {
        case MOSQ_ERR_INVAL:
            log::error() << "mqtt could not execute step: invalid";
            return -1;
        case MOSQ_ERR_NOMEM:
            log::error() << "mqtt could not execute step: memory exceeded";
            return -1;
        case MOSQ_ERR_NO_CONN:
            log::error() << "mqtt could not execute step: not connected";
            if (!connect()) {
                return -1;
            }
            break;
        case MOSQ_ERR_CONN_LOST:
            log::error() << "mqtt could not execute step: lost connection";
            if (!reconnect()) {
                return -1;
            }
            break;
        case MOSQ_ERR_PROTOCOL:
            log::error() << "mqtt could not execute step: protocol error";
            return -1;
        case MOSQ_ERR_ERRNO:
            log::error() << "mqtt could not execute step: system call error";
            return -1;
        default:
            log::error() << "mqtt could not execute step:unspecified error";
            return -1;
        }
    }

    std::this_thread::sleep_for(std::chrono::microseconds { 500 });
    return 0;
}

void mqtt::callback_connected(int result)
{
    if (result == 1) {
        log::warning() << "mqtt connection failed: Wrong protocol version";
        set_status(Status::Error);
    } else if (result == 2) {
        log::warning() << "mqtt connection failed: Credentials rejected";
        set_status(Status::Error);
    } else if (result == 3) {
        log::warning() << "mqtt connection failed: Broker unavailable";
        set_status(Status::Error);
    } else if (result > 3) {
        log::warning() << "mqtt connection failed: Other reason";
    } else if (result == 0) {
        log::info() << "Connected to mqtt.";
        set_status(Status::Connected);
        m_tries = 0;
        for (auto& [topic, sub] : m_subscribers) {
            p_subscribe(topic);
        }
        return;
    }
}

void mqtt::callback_disconnected(int result)
{
    if (result != 0) {
        log::warning() << "mqtt disconnected unexpectedly.";
        set_status(Status::Error);
    } else {
        set_status(Status::Disconnected);
    }
}

void mqtt::callback_message(const mosquitto_message* message)
{
    std::string message_topic { message->topic };
    for (auto& [topic, sub] : m_subscribers) {
        bool result {};
        mosquitto_topic_matches_sub2(topic.c_str(), topic.length(), message_topic.c_str(), message_topic.length(), &result);
        if (result) {
            sub->push_message({ message_topic, std::string { reinterpret_cast<char*>(message->payload) } });
        }
    }
}

auto mqtt::post_run() -> int
{
    m_subscribers.clear();
    m_publishers.clear();

    if (!disconnect()) {
        return -1;
    }
    if (m_mqtt != nullptr) {
        mosquitto_destroy(m_mqtt);
        m_mqtt = nullptr;
    }
    mosquitto_lib_cleanup();
    return 0;
}

auto mqtt::publish(const std::string& topic, const std::string& content) -> bool
{
    if (!check_connection()) {
        return false;
    }
    auto result { mosquitto_publish(m_mqtt, nullptr, topic.c_str(), static_cast<int>(content.size()), reinterpret_cast<const void*>(content.c_str()), 1, false) };
    if (result == MOSQ_ERR_SUCCESS) {
        return true;
    }
    log::warning() << "Could not send mqtt message: " + std::to_string(result);
    return false;
}

void mqtt::unsubscribe(const std::string& topic)
{
    if (!check_connection()) {
        return;
    }
    log::info() << "Unsubscribing from " + topic;
    mosquitto_unsubscribe(m_mqtt, nullptr, topic.c_str());
}

auto mqtt::publish(const std::string& topic) -> publisher&
{
    if (!check_connection()) {
        log::error() << "could not register mqtt publisher, not connected.";
        throw error::mqtt_could_not_publish(topic, "Not connected");
    }
    if (m_publishers.find(topic) != m_publishers.end()) {
        return { *m_publishers[topic] };
    }
    m_publishers[topic] = std::make_unique<publisher>(this, topic);
    log::info() << "Starting to publish on topic " + topic;
    return { *m_publishers[topic] };
}

auto mqtt::check_connection() -> bool
{
    if (m_status != Status::Connected) {
        if (m_status == Status::Connecting) {
            if (!wait_for(Status::Connected)) {
                log::warning() << "mqtt not connected.";
                return false;
            }
        } else {
            log::warning() << "mqtt not connected.";
            return false;
        }
    }
    return true;
}

auto mqtt::p_subscribe(const std::string& topic) -> bool
{
    auto result { mosquitto_subscribe(m_mqtt, nullptr, topic.c_str(), 1) };
    if (result != MOSQ_ERR_SUCCESS) {
        switch (result) {
        case MOSQ_ERR_INVAL:
            log::error() << "Could not subscribe to topic '" + topic + "': invalid parameters";
            break;
        case MOSQ_ERR_NOMEM:
            log::error() << "Could not subscribe to topic '" + topic + "': memory exceeded";
            break;
        case MOSQ_ERR_NO_CONN:
            log::error() << "Could not subscribe to topic '" + topic + "': Not connected";
            break;
        case MOSQ_ERR_MALFORMED_UTF8:
            log::error() << "Could not subscribe to topic '" + topic + "': malformed utf8";
            break;
        default:
            log::error() << "Could not subscribe to topic '" + topic + "': other reason";
            break;
        }
        return false;
    }
    log::info() << "Subscribed to topic '" + topic + "'.";
    return true;
}

auto mqtt::subscribe(const std::string& topic) -> subscriber&
{
    if (!check_connection()) {
        log::error() << "could not register mqtt subscriber, not connected.";
        throw error::mqtt_could_not_subscribe(topic, "Not connected");
    }

    if (m_subscribers.find(topic) != m_subscribers.end()) {
        log::info() << "Topic already subscribed.";
        return { *m_subscribers[topic] };
    }

    if (!p_subscribe(topic)) {
        log::error() << "could not register mqtt subscriber.";
        throw error::mqtt_could_not_subscribe(topic, "undisclosed error");
    }
    m_subscribers[topic] = std::make_unique<subscriber>(this, topic);
    return { *m_subscribers[topic] };
}

auto mqtt::connect() -> bool
{
    std::this_thread::sleep_for(std::chrono::seconds { 1 * m_tries });

    log::info() << "Trying to connect to MQTT.";
    m_tries++;
    set_status(Status::Connecting);

    if (m_tries > s_max_tries) {
        set_status(Status::Error);
        log::error() << "Giving up trying to connect to MQTT.";
        return false;
    }
    if (mosquitto_username_pw_set(m_mqtt, m_config.login.username.c_str(), m_config.login.password.c_str()) != MOSQ_ERR_SUCCESS) {
        log::warning() << "Could not connect to MQTT";
        return false;
    }
    auto result { mosquitto_connect(m_mqtt, m_config.host.c_str(), m_config.port, 60) };
    if (result == MOSQ_ERR_SUCCESS) {
        return true;
    }
    log::warning() << "Could not connect to MQTT: " + std::string { strerror(result) };
    return connect();
}

auto mqtt::disconnect() -> bool
{
    if (m_status != Status::Connected) {
        return true;
    }
    auto result { mosquitto_disconnect(m_mqtt) };
    if (result == MOSQ_ERR_SUCCESS) {
        set_status(Status::Disconnected);
        log::info() << "Disconnected from MQTT.";
        return true;
    }
    log::error() << "Could not disconnect from MQTT: " + std::to_string(result);
    return false;
}

auto mqtt::reconnect() -> bool
{
    std::this_thread::sleep_for(std::chrono::seconds { 1 * m_tries });

    m_tries++;
    set_status(Status::Disconnected);

    if (m_tries > (s_max_tries - 8)) {
        log::error() << "Giving up trying to reconnect to MQTT.";
        return reinitialise();
    }

    log::info() << "Trying to reconnect to MQTT.";
    auto result { mosquitto_reconnect(m_mqtt) };
    if (result == MOSQ_ERR_SUCCESS) {
        return true;
    }
    log::error() << "Could not reconnect to MQTT: " + std::to_string(result);
    return reconnect();
}

auto mqtt::reinitialise() -> bool
{
    if (m_tries > s_max_tries) {
        set_status(Status::Error);
        log::error() << "Giving up trying to reinitialise connection.";
        return false;
    }

    log::info() << "Trying to reinitialise MQTT connection.";

    if (m_mqtt != nullptr) {
        mosquitto_destroy(m_mqtt);
        m_mqtt = nullptr;
        mosquitto_lib_cleanup();
    }

    m_mqtt = init(client_id().c_str());

    mosquitto_connect_callback_set(m_mqtt, wrapper_callback_connected);
    mosquitto_disconnect_callback_set(m_mqtt, wrapper_callback_disconnected);
    mosquitto_message_callback_set(m_mqtt, wrapper_callback_message);

    if (!connect()) {
        return reinitialise();
    }

    return true;
}

void mqtt::set_status(Status status)
{
    m_status = status;
}

auto mqtt::publisher::publish(const std::string& content) -> bool
{
    return m_link->publish(m_topic, content);
}

auto mqtt::publisher::publish(const std::string& subtopic, const std::string& content) -> bool
{
    return m_link->publish(m_topic + '/' + subtopic, content);
}

auto mqtt::publisher::get_publish_topic() const -> const std::string&
{
    return m_topic;
}

void mqtt::subscriber::set_callback(std::function<void(const message_t&)> callback)
{
    m_callback.emplace_back(std::move(callback));
}

void mqtt::subscriber::push_message(const message_t& message)
{
    for (auto& callback : m_callback) {
        callback(message);
    }
}

auto mqtt::subscriber::get_subscribe_topic() const -> const std::string&
{
    return m_topic;
}

auto mqtt::client_id() const -> std::string
{
    CryptoPP::SHA1 sha1;

    std::string source { std::string { m_config.login.username } + m_config.login.station_id };
    std::string id {};
    CryptoPP::StringSource give_me_a_name { source, true, new CryptoPP::HashFilter(sha1, new CryptoPP::HexEncoder(reinterpret_cast<CryptoPP::BufferedTransformation*>(new CryptoPP::StringSink(id)))) };
    return id;
}
}
