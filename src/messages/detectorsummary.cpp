#include "messages/detectorsummary.h"

#include <utility>

namespace MuonPi {

DetectorSummary::DetectorSummary(std::size_t hash, UserInfo user_info, Data data)
    : m_hash { hash }
    , m_data { data }
    , m_userinfo { std::move(user_info) }

{
}

DetectorSummary::DetectorSummary() noexcept
    : m_valid { false }
{
}

auto DetectorSummary::data() const -> Data
{
    return m_data;
}

auto DetectorSummary::user_info() const -> UserInfo
{
    return m_userinfo;
}

auto DetectorSummary::hash() const noexcept -> std::size_t
{
    return m_hash;
}

auto DetectorSummary::time() const -> std::chrono::system_clock::time_point
{
    return m_time;
}

auto DetectorSummary::valid() const -> bool
{
    return m_valid;
}

void DetectorSummary::set_change_flag()
{
    m_data.change = 1;
}
}
