#include "link/mqtt.h"
#include "utility/log.h"

#include <functional>
#include <sstream>
#include <regex>

#include <cryptopp/sha.h>
#include <cryptopp/filters.h>
#include <cryptopp/hex.h>

namespace MuonPi::Link {


auto Mqtt::wait_for(Status status, std::chrono::milliseconds duration) -> bool
{
    std::chrono::steady_clock::time_point start { std::chrono::steady_clock::now() };
    while (((std::chrono::steady_clock::now() - start) < duration)) {
        if (m_status == status) {
            return true;
        }
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

Mqtt::Mqtt(const LoginData& login, const std::string& server, int port)
    : ThreadRunner{"Mqtt"}
    , m_host { server }
    , m_port { port }
    , m_login_data { login }
    , m_mqtt { init(login.client_id().c_str()) }
{
    mosquitto_connect_callback_set(m_mqtt, wrapper_callback_connected);
    mosquitto_disconnect_callback_set(m_mqtt, wrapper_callback_disconnected);
    mosquitto_message_callback_set(m_mqtt, wrapper_callback_message);

    start();
}

Mqtt::~Mqtt()
{
    disconnect();
    if (m_mqtt != nullptr) {
        mosquitto_destroy(m_mqtt);
        m_mqtt = nullptr;
    }
    mosquitto_lib_cleanup();
}

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
    auto status = mosquitto_loop(m_mqtt, 0, 1);
    if (status != MOSQ_ERR_SUCCESS) {
        switch (status) {
        case MOSQ_ERR_INVAL:
            Log::error()<<"Mqtt could not execute step: invalid";
            return -1;
        case MOSQ_ERR_NOMEM:
            Log::error()<<"Mqtt could not execute step: memory exceeded";
            return -1;
        case MOSQ_ERR_NO_CONN:
            Log::error()<<"Mqtt could not execute step: not connected";
            if (!connect()) {
                return -1;
            }
            break;
        case MOSQ_ERR_CONN_LOST:
            Log::error()<<"Mqtt could not execute step: lost connection";
            if (!reconnect()) {
                return -1;
            }
            break;
        case MOSQ_ERR_PROTOCOL:
            Log::error()<<"Mqtt could not execute step: protocol error";
            return -1;
        case MOSQ_ERR_ERRNO:
            Log::error()<<"Mqtt could not execute step: system call error";
            return -1;
        }
    }

    std::this_thread::sleep_for(std::chrono::microseconds{10});
    return 0;
}

void Mqtt::callback_connected(int result)
{
    if (result == 1) {
        Log::warning()<<"Mqtt connection failed: Wrong protocol version";
        set_status(Status::Error);
    } else if (result == 2) {
        Log::warning()<<"Mqtt connection failed: Credentials rejected";
        set_status(Status::Error);
    } else if (result == 3) {
        Log::warning()<<"Mqtt connection failed: Broker unavailable";
        set_status(Status::Error);
    } else if (result > 3) {
        Log::warning()<<"Mqtt connection failed: Other reason";
    } else if (result == 0) {
        Log::info()<<"Connected to mqtt.";
        set_status(Status::Connected);
        m_tries = 0;
        return;
    }
}

void Mqtt::callback_disconnected(int result)
{
    if (result != 0) {
        Log::warning()<<"Mqtt disconnected unexpectedly.";
        set_status(Status::Error);
    } else {
        set_status(Status::Disconnected);
    }
}

void Mqtt::callback_message(const mosquitto_message* message)
{
    std::string message_topic {message->topic};
    for (auto& [topic, subscriber]: m_subscribers) {
        bool result { };
        mosquitto_topic_matches_sub2(topic.c_str(), topic.length(), message_topic.c_str(), message_topic.length(), &result);
        if (result) {
            subscriber->push_message({message_topic, std::string{reinterpret_cast<char*>(message->payload)}});
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
    return 0;
}

auto Mqtt::publish(const std::string& topic, const std::string& content) -> bool
{
    if (m_status != Status::Connected) {
        if (m_status == Status::Connecting) {
            if (!wait_for(Status::Connected)) {
                return false;
            }
        } else {
            return false;
        }
    }
    auto result { mosquitto_publish(m_mqtt, nullptr, topic.c_str(), static_cast<int>(content.size()), reinterpret_cast<const void*>(content.c_str()), 1, false) };
    if (result == MOSQ_ERR_SUCCESS) {
        return true;
    }
    Log::warning()<<"Could not send Mqtt message: " + std::to_string(result);
    return false;
}

void Mqtt::unsubscribe(const std::string& topic)
{
    if (m_status != Status::Connected) {
        if (m_status == Status::Connecting) {
            if (!wait_for(Status::Connected)) {
                return;
            }
        } else {
            return;
        }
    }
    Log::info()<<"Unsubscribing from " + topic;
    mosquitto_unsubscribe(m_mqtt, nullptr, topic.c_str());
}

auto Mqtt::publish(const std::string& topic) -> Publisher&
{
    if (m_status != Status::Connected) {
        if (m_status == Status::Connecting) {
            if (!wait_for(Status::Connected)) {
                Log::warning()<<"Mqtt not connected.";
                throw -1;
            }
        } else {
            Log::warning()<<"Mqtt not connected.";
            throw -1;
        }
    }
    if (m_publishers.find(topic) != m_publishers.end()) {
        return {*m_publishers[topic]};
    }
    m_publishers[topic] = std::make_unique<Publisher>(this, topic);
    Log::debug()<<"Starting to publish on topic " + topic;
    return {*m_publishers[topic]};
}

auto Mqtt::subscribe(const std::string& topic) -> Subscriber&
{
    if (m_status != Status::Connected) {
        if (m_status == Status::Connecting) {
            if (!wait_for(Status::Connected)) {
                Log::warning()<<"Mqtt not connected.";
                throw -1;
            }
        } else {
            Log::warning()<<"Mqtt not connected.";
            throw -1;
        }
    }
    std::string check_topic { topic};
    if (m_subscribers.find(topic) != m_subscribers.end()) {
        Log::info()<<"Topic already subscribed.";
        return {*m_subscribers[topic]};
    }
    auto result { mosquitto_subscribe(m_mqtt, nullptr, topic.c_str(), 1) };
    if (result != MOSQ_ERR_SUCCESS) {
        switch (result) {
        case MOSQ_ERR_INVAL:
            Log::error()<<"Could not subscribe to topic '" + topic + "': invalid parameters";
            break;
        case MOSQ_ERR_NOMEM:
            Log::error()<<"Could not subscribe to topic '" + topic + "': memory exceeded";
            break;
        case MOSQ_ERR_NO_CONN:
            Log::error()<<"Could not subscribe to topic '" + topic + "': Not connected";
            break;
        case MOSQ_ERR_MALFORMED_UTF8:
            Log::error()<<"Could not subscribe to topic '" + topic + "': malformed utf8";
            break;
        }
        throw -1;
    }
    m_subscribers[topic] = std::make_unique<Subscriber>(this, topic);
    Log::debug()<<"Starting to subscribe to topic " + topic;
    return {*m_subscribers[topic]};
}

auto Mqtt::connect(std::size_t n) -> bool
{
    m_tries++;
    set_status(Status::Connecting);
    static constexpr std::size_t max_tries { 5 };

    if ((n > max_tries) || (m_tries > max_tries*2)) {
        set_status(Status::Error);
        Log::error()<<"Giving up trying to connect to MQTT.";
        return false;
    }
    if (mosquitto_username_pw_set(m_mqtt, m_login_data.username.c_str(), m_login_data.password.c_str()) != MOSQ_ERR_SUCCESS) {
        Log::warning()<<"Could not connect to MQTT";
        return false;
    }
    auto result { mosquitto_connect(m_mqtt, m_host.c_str(), m_port, 60) };
    if (result == MOSQ_ERR_SUCCESS) {
        return true;
    }
    std::this_thread::sleep_for( std::chrono::seconds{1} );
    Log::warning()<<"Could not connect to MQTT: " + std::to_string(result);
    return connect(n + 1);
}

auto Mqtt::disconnect() -> bool
{
    if (m_status != Status::Connected) {
        return true;
    }
    auto result { mosquitto_disconnect(m_mqtt) };
    if (result == MOSQ_ERR_SUCCESS) {
        set_status(Status::Disconnected);
        Log::info()<<"Disconnected from MQTT.";
        return true;
    }
    Log::error()<<"Could not disconnect from MQTT: " + std::to_string(result);
    return false;
}

auto Mqtt::reconnect(std::size_t n) -> bool
{
    m_tries++;
    set_status(Status::Disconnected);
    static constexpr std::size_t max_tries { 5 };

    if ((n > max_tries) || (m_tries > max_tries*2)) {
        set_status(Status::Error);
        Log::error()<<"Giving up trying to reconnect to MQTT.";
        return false;
    }

    Log::info()<<"Trying to reconnect to MQTT.";
    auto result { mosquitto_reconnect(m_mqtt) };
    if (result == MOSQ_ERR_SUCCESS) {
        return true;
    }
    std::this_thread::sleep_for( std::chrono::seconds{1} );
    Log::error()<<"Could not reconnect to MQTT: " + std::to_string(result);
    return reconnect(n + 1);
}

void Mqtt::set_status(Status status) {
    m_status = status;
}

auto Mqtt::Publisher::publish(const std::string& content) -> bool
{
    return m_link->publish(m_topic, content);
}

auto Mqtt::Publisher::publish(const std::string& subtopic, const std::string& content) -> bool
{
    std::string topic { m_topic };

    const bool sub_slash { subtopic[0] != '/' };
    const bool topic_slash { (*m_topic.end()) != '/' };

    if ( sub_slash ^ topic_slash ) {
        topic += subtopic;
    } else if( sub_slash | topic_slash ) {
        if (subtopic.length() < 2) {
            return false;
        }
        topic += subtopic.substr(1, subtopic.size() - 1);
    } else {
        topic += '/' + subtopic;
    }

    return m_link->publish(topic , content);
}

auto Mqtt::Publisher::get_publish_topic() const -> const std::string& {
    return m_topic;
}

void Mqtt::Subscriber::set_callback(std::function<void(const Message&)> callback)
{
    m_callback = std::move(callback);
}

void Mqtt::Subscriber::push_message(const Message &message)
{
    m_callback(message);
}

auto Mqtt::Subscriber::get_subscribe_topic() const -> const std::string& {
    return m_topic;
}

auto Mqtt::LoginData::client_id() const -> std::string
{
    CryptoPP::SHA1 sha1;

    std::string source {username + station_id};
    std::string id {};
    CryptoPP::StringSource{source, true, new CryptoPP::HashFilter(sha1, new CryptoPP::HexEncoder(new CryptoPP::StringSink(id)))};
    return id;
}
}