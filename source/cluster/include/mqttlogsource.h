#ifndef MQTTLOGSOURCE_H
#define MQTTLOGSOURCE_H

#include "abstractsource.h"
#include "mqttlink.h"
#include "logmessage.h"

#include <map>
#include <memory>
#include <map>

namespace MuonPi {

class LogMessage;
class MessageParser;

struct LogItem {
    static constexpr std::uint8_t s_default_status { 0xFF };
    std::string id {};
    std::uint8_t status { s_default_status };

    struct {
        double h;
        double lat;
        double lon;
        double h_acc;
        double v_acc;
        double dop;
    } geo;

    struct {
        double accuracy;
        double dop;
    } time;

    void reset();

    auto add(MessageParser& message) -> bool;

    [[nodiscard]] auto complete() -> bool;

struct PartialLogEntry {
	LogMessage logmessage { LogMessage(std::size_t {}, Location {}) };
	std::uint8_t completeness {0x00};
	static constexpr std::uint8_t max_completeness { 0x1f };
};

/**
 * @brief The MqttLogSource class
 */
class MqttLogSource : public AbstractSource<LogMessage>
{
public:
    /**
     * @brief MqttLogSource
     * @param subscriber The Mqtt Topic this Log source should be subscribed to
     */
    MqttLogSource(MqttLink::Subscriber& subscriber);

    ~MqttLogSource() override;

    auto pre_run() -> int override;

protected:
    /**
     * @brief step implementation from ThreadRunner
     * @return zero if the step succeeded.
     */
    [[nodiscard]] auto step() -> int override;

private:
    MqttLink::Subscriber& m_link;

    void process(std::size_t hash, LogItem item);

    std::map<std::size_t, LogItem> m_buffer {};
};

}

#endif // MQTTLOGSOURCE_H
