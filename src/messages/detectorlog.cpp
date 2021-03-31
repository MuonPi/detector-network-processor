#include "messages/detectorlog.h"

#include <utility>

namespace muonpi {

void detector_log_t::add_item(detector_log_item item)
{
    if (item == detector_log_item {}) {
        return;
    }
    m_items.emplace(item);
}

auto detector_log_t::has_items() const -> bool
{
    return (!m_items.empty());
}

auto detector_log_t::next_item() -> detector_log_item
{
    if (!has_items()) {
        return detector_log_item {};
    }
    detector_log_item item { m_items.front() };
    m_items.pop();
    return item;
}

auto detector_log_t::log_id() const -> const std::string&
{
    return m_log_id;
}

void detector_log_t::set_userinfo(userinfo_t user_info)
{
    m_userinfo = std::move(user_info);
}

void detector_log_t::set_log_id(const std::string& log_id)
{
    m_log_id = log_id;
}

auto detector_log_t::user_info() const -> userinfo_t
{
    return m_userinfo;
}

}
