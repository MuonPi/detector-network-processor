#include "messages/detectorlog.h"

namespace MuonPi {

void DetectorLog::add_item(DetectorLogItem item)
{
	if ( item == DetectorLogItem {} ) return;
	m_items.emplace(item);
}

auto DetectorLog::has_items() const -> bool
{
	return (!m_items.empty());
}

auto DetectorLog::next_item() -> DetectorLogItem
{
	if (!has_items()) return DetectorLogItem {};
	DetectorLogItem item { m_items.front() };
	m_items.pop();
	return item;
}

auto DetectorLog::log_id() const -> const std::string&
{
	return m_log_id;
}

void DetectorLog::set_userinfo(UserInfo user_info)
{
	m_userinfo = user_info;
}

void DetectorLog::set_log_id(const std::string& log_id)
{
	m_log_id = log_id;
}


}
