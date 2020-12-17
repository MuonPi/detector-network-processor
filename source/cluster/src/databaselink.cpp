#include "databaselink.h"

#include "log.h"

namespace MuonPi {
DbEntry::DbEntry(const std::string& measurement)
    : m_measurement { measurement }
{}

DatabaseLink::DatabaseLink(const std::string& server, const LoginData& login, const std::string& database)
    : m_server { server }
    , m_login_data { login }
    , m_database { database }
    , m_server_info { influxdb_cpp::server_info(m_server, 8086, m_database, m_login_data.username, m_login_data.password) }
{

}

DatabaseLink::~DatabaseLink() = default;


auto DatabaseLink::write_entry(const DbEntry& entry) -> bool
{
    influxdb_cpp::builder builder{};
    influxdb_cpp::detail::tag_caller& tags { builder.meas(entry.measurement()) };

    if (!entry.tags().empty()) {
        for (auto& [key, value] : entry.tags()) {
            tags.tag(key, value);
        }
    }

    if (entry.fields().empty()) {
        return false;
    }
    std::vector<std::pair<std::string, std::string>> field_list { entry.fields() };
    auto& fields {tags.field(field_list[0].first, field_list[0].second)};
    field_list.erase(field_list.begin());
    for (auto& [key, value] : field_list) {
        fields.field(key, value);
    }

    std::scoped_lock<std::mutex> lock { m_mutex };

    int result = fields.timestamp(stoull(entry.timestamp(), nullptr))
            .post_http(m_server_info);

    if (result == -3) {
        Log::warning()<<"Database not connected.";
    } else if (result != 0) {
        Log::warning()<<"Unspecified database error: " + std::to_string(result);
    }

    return result == 0;
}

} // namespace MuonPi
