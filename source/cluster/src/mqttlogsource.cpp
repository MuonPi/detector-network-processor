#include "mqttlogsource.h"

#include "logmessage.h"
#include "utility.h"
#include "log.h"

namespace MuonPi {

void LogItem::reset()
{
    id = "";
    status = s_default_status;
}

auto LogItem::add(MessageParser& message) -> bool
{
    if (id != message[0]) {
        Log::debug()<<"Resetting log aggregation.";
        reset();
        id = message[0];
    }
    if (message[1] == "geoHeightMSL") {
        geo.h = std::stod(message[2], nullptr);
        status &= ~1;
    } else if (message[1] == "geoHorAccuracy") {
        geo.h_acc = std::stod(message[2], nullptr);
        status &= ~2;
    } else if (message[1] == "geoLatitude") {
        geo.lat = std::stod(message[2], nullptr);
        status &= ~4;
    } else if (message[1] == "geoLongitude") {
        geo.lon = std::stod(message[2], nullptr);
        status &= ~8;
    } else if (message[1] == "geoVertAccuracy") {
        geo.v_acc = std::stod(message[2], nullptr);
        status &= ~16;
    } else if (message[1] == "positionDOP") {
        geo.dop = std::stod(message[2], nullptr);
        status &= ~32;
    } else if (message[1] == "timeAccuracy") {
        time.accuracy = std::stod(message[2], nullptr);
        status &= ~64;
    } else if (message[1] == "timeDOP") {
        time.dop = std::stod(message[2], nullptr);
        status &= ~128;
    } else {
        return false;
    }
    return true;
}

auto LogItem::complete() -> bool
{
    return !status;
}

MqttLogSource::MqttLogSource(std::shared_ptr<MqttLink::Subscriber> subscriber)
    : m_link { subscriber }
{
    start();
}

MqttLogSource::~MqttLogSource() = default;

auto MqttLogSource::pre_run() -> int
{
    if (m_link == nullptr) {
        return -1;
    }
    return 0;
}

auto MqttLogSource::step() -> int
{
	if (m_link->has_message()) {
		MqttLink::Message msg = m_link->get_message();
        MessageParser topic { msg.topic, '/'};
        MessageParser content { msg.content, ' '};
		
        // todo: parsing of message
		if ((topic.size() == 4) && (content.size() > 2)) {
			std::size_t hash {std::hash<std::string>{}(topic[2] + topic[3])};
			if (m_logbuffer.find(hash)!=std::end(m_logbuffer)) {
				// the detector already exists, so we assume that other logdata was sent before and will complement the entry
				static std::uint8_t _location_complete_flags = 0x00;
				LogMessage logmessage ( m_logbuffer.at(hash) );
				std::string parname = content[1];
				std::string valstring = content[2];
				Location location { logmessage.location() };
				if (parname.find("geoLatitude") != std::string::npos) {
					location.lat = std::stod(valstring, nullptr);
					_location_complete_flags |= 0x01;
				} else 
				if (parname.find("geoLongitude") != std::string::npos) {
					location.lon = std::stod(valstring, nullptr);
					_location_complete_flags |= 0x02;
				} else 
				if (parname.find("geoHeight") != std::string::npos) {
					location.h = std::stod(valstring, nullptr);
					_location_complete_flags |= 0x04;
				} else 
				if (parname.find("geoHorAccuracy") != std::string::npos) {
					location.prec = std::stod(valstring, nullptr);
					_location_complete_flags |= 0x08;
				} else 
				if (parname.find("positionDOP") != std::string::npos) {
					location.dop = std::stod(valstring, nullptr);
					_location_complete_flags |= 0x10;
				} else {
					// nothing of interest in the message, go out
					return 0;
				}
				
				//if (logmessage.hash() == hash) logmessage=LogMessage(std::size_t{}, Location{});
				
				if (_location_complete_flags == 0x1f) {
					push_item(std::make_unique<LogMessage>(LogMessage(hash, location)));
					m_logbuffer.erase(hash);
					_location_complete_flags=0x00;
				}
			} else {
				// the detector is unknown, so create a new LogMessage entry in the buffer
				m_logbuffer.at(hash) = LogMessage(hash, Location{});
			}
		}
    }
    std::this_thread::sleep_for(std::chrono::microseconds{50});
    return 0;
}
}
