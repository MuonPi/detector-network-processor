#include "link/mqtt.h"
#include "utility/log.h"

#include <cstring>
#include <functional>
#include <regex>
#include <sstream>
#include <utility>

#include <cryptopp/filters.h>
#include <cryptopp/hex.h>
#include <cryptopp/sha.h>

namespace MuonPi::Link {

auto Mqtt::wait_for(Status status, std::chrono::milliseconds duration) -> bool
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
    reinterpret_cast<Mqtt*>(object)->callback_connected(result);
}

void wrapper_callback_disconnected(mosquitto* /*mqtt*/, void* object, int result)
{
    reinterpret_cast<Mqtt*>(object)->callback_disconnected(result);
}

void wrapper_callback_message(mosquitto* /*mqtt*/, void* object, const mosquitto_message* message)
{
    reinterpret_cast<Mqtt*>(object)->callback_message(message);
}

Mqtt::Mqtt(Config::Mqtt config)
    : ThreadRunner { "Mqtt" }
    , m_config { std::move(config) }
    , m_mqtt { init(client_id().c_str()) }
{
    mosquitto_connect_callback_set(m_mqtt, wrapper_callback_connected);
    mosquitto_disconnect_callback_set(m_mqtt, wrapper_callback_disconnected);
    mosquitto_message_callback_set(m_mqtt, wrapper_callback_message);

    start();
}

Mqtt::Mqtt()
    : ThreadRunner("Mqtt")
    , m_mqtt { init(client_id().c_str()) }
{
}

Mqtt::~Mqtt()
    = default;

auto Mqtt::pre_run() -> int
{
    if (!connect()) {
        return -1;
    }
    if ((m_status != Status::Connected) && (m_status != Status::Connecting)) {
        return -1;
    }
    return 0;
}

auto Mqtt::step() -> int
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
            Log::error() << "Mqtt could not execute step: invalid";
            return -1;
        case MOSQ_ERR_NOMEM:
            Log::error() << "Mqtt could not execute step: memory exceeded";
            return -1;
        case MOSQ_ERR_NO_CONN:
            Log::error() << "Mqtt could not execute step: not connected";
            if (!connect()) {
                return -1;
            }
            break;
        case MOSQ_ERR_CONN_LOST:
            Log::error() << "Mqtt could not execute step: lost connection";
            if (!reconnect()) {
                return -1;
            }
            break;
        case MOSQ_ERR_PROTOCOL:
            Log::error() << "Mqtt could not execute step: protocol error";
            return -1;
        case MOSQ_ERR_ERRNO:
            Log::error() << "Mqtt could not execute step: system call error";
            return -1;
        default:
            Log::error() << "Mqtt could not execute step:unspecified error";
            return -1;
        }
    }

    std::this_thread::sleep_for(std::chrono::microseconds { 500 });
    return 0;
}

void Mqtt::callback_connected(int result)
{
    if (result == 1) {
        Log::warning() << "Mqtt connection failed: Wrong protocol version";
        set_status(Status::Error);
    } else if (result == 2) {
        Log::warning() << "Mqtt connection failed: Credentials rejected";
        set_status(Status::Error);
    } else if (result == 3) {
        Log::warning() << "Mqtt connection failed: Broker unavailable";
        set_status(Status::Error);
    } else if (result > 3) {
        Log::warning() << "Mqtt connection failed: Other reason";
    } else if (result == 0) {
        Log::info() << "Connected to mqtt.";
        set_status(Status::Connected);
        m_tries = 0;
        for (auto& [topic, subscriber] : m_subscribers) {
            p_subscribe(topic);
        }
        return;
    }
}

void Mqtt::callback_disconnected(int result)
{
    if (result != 0) {
        Log::warning() << "Mqtt disconnected unexpectedly.";
        set_status(Status::Error);
    } else {
        set_status(Status::Disconnected);
    }
}

void Mqtt::callback_message(const mosquitto_message* message)
{
    std::string message_topic { message->topic };
    for (auto& [topic, subscriber] : m_subscribers) {
        bool result {};
        mosquitto_topic_matches_sub2(topic.c_str(), topic.length(), message_topic.c_str(), message_topic.length(), &result);
        if (result) {
            subscriber->push_message({ message_topic, std::string { reinterpret_cast<char*>(message->payload) } });
        }
    }
}

auto Mqtt::post_run() -> int
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

auto Mqtt::publish(const std::string& topic, const std::string& content) -> bool
{
    if (!check_connection()) {
        return false;
    }
    auto result { mosquitto_publish(m_mqtt, nullptr, topic.c_str(), static_cast<int>(content.size()), reinterpret_cast<const void*>(content.c_str()), 1, false) };
    if (result == MOSQ_ERR_SUCCESS) {
        return true;
    }
    Log::warning() << "Could not send Mqtt message: " + std::to_string(result);
    return false;
}

void Mqtt::unsubscribe(const std::string& topic)
{
    if (!check_connection()) {
        return;
    }
    Log::info() << "Unsubscribing from " + topic;
    mosquitto_unsubscribe(m_mqtt, nullptr, topic.c_str());
}

auto Mqtt::publish(const std::string& topic) -> Publisher&
{
    if (!check_connection()) {
        throw -1;
    }
    if (m_publishers.find(topic) != m_publishers.end()) {
        return { *m_publishers[topic] };
    }
    m_publishers[topic] = std::make_unique<Publisher>(this, topic);
    Log::info() << "Starting to publish on topic " + topic;
    return { *m_publishers[topic] };
}

auto Mqtt::check_connection() -> bool
{
    if (m_status != Status::Connected) {
        if (m_status == Status::Connecting) {
            if (!wait_for(Status::Connected)) {
                Log::warning() << "Mqtt not connected.";
                return false;
            }
        } else {
            Log::warning() << "Mqtt not connected.";
            return false;
        }
    }
    return true;
}

auto Mqtt::p_subscribe(const std::string& topic) -> bool
{
    auto result { mosquitto_subscribe(m_mqtt, nullptr, topic.c_str(), 1) };
    if (result != MOSQ_ERR_SUCCESS) {
        switch (result) {
        case MOSQ_ERR_INVAL:
            Log::error() << "Could not subscribe to topic '" + topic + "': invalid parameters";
            break;
        case MOSQ_ERR_NOMEM:
            Log::error() << "Could not subscribe to topic '" + topic + "': memory exceeded";
            break;
        case MOSQ_ERR_NO_CONN:
            Log::error() << "Could not subscribe to topic '" + topic + "': Not connected";
            break;
        case MOSQ_ERR_MALFORMED_UTF8:
            Log::error() << "Could not subscribe to topic '" + topic + "': malformed utf8";
            break;
        default:
            Log::error() << "Could not subscribe to topic '" + topic + "': other reason";
            break;
        }
        return false;
    }
    Log::info() << "Subscribed to topic '" + topic + "'.";
    return true;
}

auto Mqtt::subscribe(const std::string& topic) -> Subscriber&
{
    if (!check_connection()) {
        throw -1;
    }

    std::string check_topic { topic };
    if (m_subscribers.find(topic) != m_subscribers.end()) {
        Log::info() << "Topic already subscribed.";
        return { *m_subscribers[topic] };
    }

    if (!p_subscribe(topic)) {
        throw -1;
    }
    m_subscribers[topic] = std::make_unique<Subscriber>(this, topic);
    return { *m_subscribers[topic] };
}

auto Mqtt::connect() -> bool
{
    std::this_thread::sleep_for(std::chrono::seconds { 1 * m_tries });

    Log::info() << "Trying to connect to MQTT.";
    m_tries++;
    set_status(Status::Connecting);

    if (m_tries > s_max_tries) {
        set_status(Status::Error);
        Log::error() << "Giving up trying to connect to MQTT.";
        return false;
    }
    if (mosquitto_username_pw_set(m_mqtt, m_config.login.username.c_str(), m_config.login.password.c_str()) != MOSQ_ERR_SUCCESS) {
        Log::warning() << "Could not connect to MQTT";
        return false;
    }
    auto result { mosquitto_connect(m_mqtt, m_config.host.c_str(), m_config.port, 60) };
    if (result == MOSQ_ERR_SUCCESS) {
        return true;
    }
    Log::warning() << "Could not connect to MQTT: " + std::string { strerror(result) };
    return connect();
}

auto Mqtt::disconnect() -> bool
{
    if (m_status != Status::Connected) {
        return true;
    }
    auto result { mosquitto_disconnect(m_mqtt) };
    if (result == MOSQ_ERR_SUCCESS) {
        set_status(Status::Disconnected);
        Log::info() << "Disconnected from MQTT.";
        return true;
    }
    Log::error() << "Could not disconnect from MQTT: " + std::to_string(result);
    return false;
}

auto Mqtt::reconnect() -> bool
{
    std::this_thread::sleep_for(std::chrono::seconds { 1 * m_tries });

    m_tries++;
    set_status(Status::Disconnected);

    if (m_tries > (s_max_tries - 8)) {
        Log::error() << "Giving up trying to reconnect to MQTT.";
        return reinitialise();
    }

    Log::info() << "Trying to reconnect to MQTT.";
    auto result { mosquitto_reconnect(m_mqtt) };
    if (result == MOSQ_ERR_SUCCESS) {
        return true;
    }
    Log::error() << "Could not reconnect to MQTT: " + std::to_string(result);
    return reconnect();
}

auto Mqtt::reinitialise() -> bool
{
    if (m_tries > s_max_tries) {
        set_status(Status::Error);
        Log::error() << "Giving up trying to reinitialise connection.";
        return false;
    }

    Log::info() << "Trying to reinitialise MQTT connection.";

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

void Mqtt::set_status(Status status)
{
    m_status = status;
}

auto Mqtt::Publisher::publish(const std::string& content) -> bool
{
    return m_link->publish(m_topic, content);
}

auto Mqtt::Publisher::publish(const std::string& subtopic, const std::string& content) -> bool
{
    return m_link->publish(m_topic + '/' + subtopic, content);
}

auto Mqtt::Publisher::get_publish_topic() const -> const std::string&
{
    return m_topic;
}

void Mqtt::Subscriber::set_callback(std::function<void(const Message&)> callback)
{
    m_callback.emplace_back(std::move(callback));
}

void Mqtt::Subscriber::push_message(const Message& message)
{
    for (auto& callback : m_callback) {
        callback(message);
    }
}

auto Mqtt::Subscriber::get_subscribe_topic() const -> const std::string&
{
    return m_topic;
}

auto Mqtt::client_id() const -> std::string
{
    CryptoPP::SHA1 sha1;

    std::string source { std::string { m_config.login.username } + m_config.login.station_id };
    std::string id {};
    CryptoPP::StringSource give_me_a_name { source, true, new CryptoPP::HashFilter(sha1, new CryptoPP::HexEncoder(reinterpret_cast<CryptoPP::BufferedTransformation*>(new CryptoPP::StringSink(id)))) };
    return id;
}
}
