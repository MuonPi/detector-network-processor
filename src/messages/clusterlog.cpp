#include "messages/clusterlog.h"

#include <utility>

namespace muonpi {

cluster_log_t::cluster_log_t(Data data)
    : m_data { std::move(data) }
{
}

auto cluster_log_t::data() const -> Data
{
    return m_data;
}

auto cluster_log_t::time() const -> std::chrono::system_clock::time_point
{
    return m_time;
}

auto cluster_log_t::user_info() const -> userinfo_t
{
    return m_userinfo;
}

}
