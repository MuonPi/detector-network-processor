#ifndef DETECTORLOG_H
#define DETECTORLOG_H

#include "messages/userinfo.h"

#include <chrono>
#include <string>
#include <list>
#include <variant>

namespace MuonPi {


struct DetectorLogItem
{
	std::string name {};
	std::variant<std::string, bool, std::int_fast64_t, double, std::size_t, std::uint8_t, std::uint16_t, std::uint32_t> value;
	bool operator==(DetectorLogItem other) { return (name==other.name && value==other.value); } 
};

class DetectorLog {
public:
	void add_item(DetectorLogItem item);
	auto has_items() const -> bool;
	auto next_item() -> DetectorLogItem;
	auto log_id() const -> const std::string&;
	void set_userinfo(UserInfo user_info);
	void set_log_id(const std::string& log_id);
private:	
	UserInfo m_userinfo { };
	std::string m_log_id { };
	std::list<DetectorLogItem> m_items { };
};


}

#endif // DETECTORLOG_H
