#ifndef MQTTLINK_H
#define MQTTLINK_H

#include "defaults.h"
#include "utility/threadrunner.h"

#include <string>
#include <memory>
#include <chrono>
#include <map>
#include <future>
#include <regex>
#include <queue>

#include <mosquitto.h>

namespace MuonPi::Link {


/**
 * @brief The Mqtt class. Connects to a Mqtt server and offers publish and subscribe methods.
 */
class Mqtt : public ThreadRunner
{
public:

    enum class Status {
        Invalid,
        Connected,
        Disconnected,
        Connecting,
        Error
    };
    struct Message
    {
        Message() = default;
        Message(const std::string& a_topic, const std::string& a_content)
        : topic { a_topic }, content {a_content }
        {}
        std::string topic {};
        std::string content{};
    };

    /**
     * @brief The Publisher class. Only gets instantiated from within the Mqtt class.
     */
    class Publisher {
    public:
        Publisher(Mqtt* link, const std::string& topic)
            : m_link { link }
            , m_topic { topic }
        {}

        /**
         * @brief publish Publish a message
         * @param content The content to send
         * @return true if the sending was successful
         */
        [[nodiscard]] auto publish(const std::string& content) -> bool;

        /**
         * @brief publish Publish a message
         * @param subtopic Subtopic to add to the basetopic specified in the constructor
         * @param content The content to send
         * @return true if the sending was successful
         */
        [[nodiscard]] auto publish(const std::string& subtopic, const std::string& content) -> bool;

        /**
         * @brief get_publish_topic Gets the topic under which the publisher publishes messages
         * @return a std::string containing the publish topic
         */
        [[nodiscard]] auto get_publish_topic() const -> const std::string&;

        Publisher() = default;
    private:
        friend class Mqtt;

        Mqtt* m_link { nullptr };
        std::string m_topic {};
    };

    /**
     * @brief The Subscriber class. Only gets instantiated from within the Mqtt class.
     */
    class Subscriber {
    public:
        Subscriber(Mqtt* link, const std::string& topic)
            : m_link { link }
            , m_topic { topic }
        {}

        ~Subscriber()
        {
            m_link->unsubscribe(m_topic);
        }

        Subscriber() = default;

        void set_callback(std::function<void(const Message&)> callback);

        /**
         * @brief get_subscribe_topic Gets the topic the subscriber subscribes to
         * @return a std::string containing the subscribed topic
         */
        [[nodiscard]] auto get_subscribe_topic() const -> const std::string&;
    private:
        friend class Mqtt;

        /**
         * @brief push_message Only called from within the Mqtt class
         * @param message The message to push into the queue
         */
        void push_message(const Message& message);

        Mqtt* m_link { nullptr };
        std::string m_topic {};
        std::function<void(const Message&)> m_callback;
    };



    /**
     * @brief Mqtt
     * @param config The configuration to use
     */
    Mqtt(const Config::Mqtt& config);

    Mqtt();

    ~Mqtt() override;


    /**
     * @brief publish Create a Publisher callback object
     * @param topic The topic under which the Publisher sends messages
     */
    [[nodiscard]] auto publish(const std::string& topic) -> Publisher&;

    /**
     * @brief subscribe Create a Subscriber callback object
     * @param topic The topic to subscribe to. See mqtt for wildcards.
     */
    [[nodiscard]] auto subscribe(const std::string& topic) -> Subscriber&;

    /**
     * @brief wait_for Wait for a designated time until the status changes to the one set as the parameter
     * @param status The status to wait for
     * @param duration The duration to wait for as a maximum
     */
    [[nodiscard]] auto wait_for(Status status, std::chrono::milliseconds duration = std::chrono::seconds{5}) -> bool;
protected:
    /**
     * @brief pre_run Reimplemented from ThreadRunner
     * @return 0 if the thread should start
     */
    [[nodiscard]] auto pre_run() -> int override;
    /**
     * @brief step Reimplemented from ThreadRunner
     * @return 0 if the thread should continue running
     */
    [[nodiscard]] auto step() -> int override;
    /**
     * @brief post_run Reimplemented from ThreadRunner
     * @return The return value of the thread loop
     */
    [[nodiscard]] auto post_run() -> int override;

private:

    /**
     * @brief set_status Set the status for this Mqtt
     * @param status The new status
     */
    void set_status(Status status);

    [[nodiscard]] auto publish(const std::string& topic, const std::string& content) -> bool;

    /**
     * @brief unsubscribe Unsubscribe from a specific topic
     * @param topic The topic string to unsubscribe from
     */
    void unsubscribe(const std::string& topic);

    /**
     * @brief connects to the Server synchronuously. This method blocks until it is connected.
     * @return true if the connection was successful
     */
    [[nodiscard]] auto connect() -> bool;

    /**
     * @brief disconnect Disconnect from the server
     * @return true if the disconnect was successful
     */
    auto disconnect() -> bool;

    /**
     * @brief reconnect attempt a reconnect after the connection was lost.
     * @return true if the reconnect was successful.
     */
    [[nodiscard]] auto reconnect() -> bool;

    /**
     * @brief reconnect attempt a reconnect after the connection was lost.
     * @return true if the reconnect was successful.
     */
    [[nodiscard]] auto reinitialise() -> bool;

    [[nodiscard]] auto check_connection() -> bool;

    auto p_subscribe(const std::string& topic) -> bool;

    /**
     * @brief init Initialise the mosquitto object. This is necessary since the mosquitto_lib_init() needs to be called before mosquitto_new().
     * @param client_id The client_id to use
     */
    [[nodiscard]] inline auto init(const char* client_id) -> mosquitto*
    {
        mosquitto_lib_init();
        return mosquitto_new(client_id, true, this);
    }

    Config::Mqtt m_config { Config::mqtt };
    mosquitto *m_mqtt { nullptr };

    Status m_status { Status::Invalid };

    std::map<std::string, std::unique_ptr<Publisher>> m_publishers {};
    std::map<std::string, std::unique_ptr<Subscriber>> m_subscribers {};

    std::size_t m_tries { 0 };
    static constexpr std::size_t s_max_tries { 10 };

    /**
     * @brief callback_connected Gets called by mosquitto client
     * @param result The status code from the callback
     */
    void callback_connected(int result);

    /**
     * @brief callback_disconnected Gets called by mosquitto client
     * @param result The status code from the callback
     */
    void callback_disconnected(int result);

    /**
     * @brief callback_message Gets called by mosquitto client in the case of an arriving message
     * @param message A const pointer to the received message
     */
    void callback_message(const mosquitto_message* message);

    /**
     * @brief client_id Creates a client_id from the username and the station id.
     * This hashes the concatenation of the two fields.
     * @return The client id as string
     */
    [[nodiscard]] auto client_id() const -> std::string;

    friend void wrapper_callback_connected(mosquitto* mqtt, void* object, int result);
    friend void wrapper_callback_disconnected(mosquitto* mqtt, void* object, int result);
    friend void wrapper_callback_message(mosquitto* mqtt, void* object, const mosquitto_message* message);
};


}

#endif // MQTTLINK_H
