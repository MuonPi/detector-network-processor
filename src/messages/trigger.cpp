#include "messages/trigger.h"

#include "utility/utility.h"

#include <sstream>

namespace muonpi::trigger {

auto detector::setting_t::to_string(char delimiter) const -> std::string
{
    std::ostringstream stream;
    if (delimiter == 0) {
        stream << username << station;
    } else {
        stream << username << delimiter << station << delimiter;
    }
    switch (type) {
    case trigger::detector::setting_t::Offline:
        stream << "offline";
        break;
    case trigger::detector::setting_t::Online:
        stream << "online";
        break;
    case trigger::detector::setting_t::Unreliable:
        stream << "unreliable";
        break;
    case trigger::detector::setting_t::Reliable:
        stream << "reliable";
        break;
    case trigger::detector::setting_t::Invalid:
        stream << "invalid";
        break;
    }
    return stream.str();
}

auto detector::setting_t::id() const -> std::size_t
{
    return std::hash<std::string> {}(to_string());
}

auto detector::setting_t::from_string(const std::string& string) -> setting_t
{
    message_parser parser { string, ' ' };

    if (parser.size() != 3) {
        return setting_t {};
    }

    setting_t trigger;
    trigger.username = parser[0];
    trigger.station = parser[1];
    if (parser[2] == "offline") {
        trigger.type = trigger::detector::setting_t::Offline;
    } else if (parser[2] == "online") {
        trigger.type = trigger::detector::setting_t::Online;
    } else if (parser[2] == "unreliable") {
        trigger.type = trigger::detector::setting_t::Unreliable;
    } else if (parser[2] == "reliable") {
        trigger.type = trigger::detector::setting_t::Reliable;
    } else {
        return setting_t {};
    }
    return trigger;
}

}
