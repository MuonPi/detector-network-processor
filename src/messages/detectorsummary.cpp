#include "messages/detectorsummary.h"

#include <utility>

namespace muonpi {

detetor_summary_t::detetor_summary_t(std::size_t hash, userinfo_t user_info, data_t data)
    : m_hash { hash }
    , m_data { data }
    , m_userinfo { std::move(user_info) }

{
}

detetor_summary_t::detetor_summary_t() noexcept
    : m_valid { false }
{
}

auto detetor_summary_t::data() const -> data_t
{
    return m_data;
}

auto detetor_summary_t::user_info() const -> userinfo_t
{
    return m_userinfo;
}

auto detetor_summary_t::hash() const noexcept -> std::size_t
{
    return m_hash;
}

auto detetor_summary_t::time() const -> std::chrono::system_clock::time_point
{
    return m_time;
}

auto detetor_summary_t::valid() const -> bool
{
    return m_valid;
}

void detetor_summary_t::set_change_flag()
{
    m_data.change = 1;
}
}
