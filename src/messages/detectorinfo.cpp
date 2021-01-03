#include "messages/detectorinfo.h"

#include <utility>

namespace MuonPi {

auto DetectorType::id() const -> std::uint8_t
{
	return 0;
}
	
DetectorInfo::DetectorInfo(std::size_t hash, /* std::string msg_time,*/ UserInfo user_info, Location location)
    : m_hash { hash }
    , m_location { location }
    , m_userinfo { user_info }
{
}

DetectorInfo::DetectorInfo() noexcept
    : m_valid { false }
{}

auto DetectorInfo::hash() const noexcept -> std::size_t
{
    return m_hash;
}


auto DetectorInfo::location() const -> Location
{
    return m_location;
}

auto DetectorInfo::user_info() const -> UserInfo
{
    return m_userinfo;
}

auto DetectorInfo::time() const -> std::chrono::system_clock::time_point
{
    return m_time;
}
/*
auto DetectorInfo::valid() const -> bool
{
    return m_valid;
}
*/
}
