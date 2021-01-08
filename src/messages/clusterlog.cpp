#include "messages/clusterlog.h"

#include <utility>

namespace MuonPi {

ClusterLog::ClusterLog(Data data)
    : m_data {std::move( data )}
{}


auto ClusterLog::data() const -> Data
{
    return m_data;
}

auto ClusterLog::time() const -> std::chrono::system_clock::time_point
{
    return m_time;
}

auto ClusterLog::user_info() const -> UserInfo
{
    return m_userinfo;
}

}
