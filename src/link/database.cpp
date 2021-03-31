#include "link/database.h"

#include "utility/log.h"
#include "utility/scopeguard.h"

#include <type_traits>
#include <utility>
#include <variant>

#include <curl/curl.h>

namespace muonpi::link {
database::entry::entry(const std::string& measurement, database& link)
    : m_link { link }
{
    m_tags << measurement;
}

auto database::entry::operator<<(const influx::tag& tag) -> entry&
{
    m_tags << ',' << tag.name << '=' << tag.field;
    return *this;
}

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

auto database::entry::operator<<(const influx::field& field) -> entry&
{
    std::visit(overloaded {
                   [this, field](const std::string& value) { m_fields << ',' << field.name << "=\"" << value << '"'; },
                   [this, field](std::int_fast64_t value) { m_fields << ',' << field.name << '=' << value << 'i'; },
                   [this, field](std::size_t value) { m_fields << ',' << field.name << '=' << value << 'i'; },
                   [this, field](std::uint8_t value) { m_fields << ',' << field.name << '=' << static_cast<std::uint16_t>(value) << 'i'; },
                   [this, field](std::uint16_t value) { m_fields << ',' << field.name << '=' << value << 'i'; },
                   [this, field](std::uint32_t value) { m_fields << ',' << field.name << '=' << value << 'i'; },
                   [this, field](bool value) { m_fields << field.name << ',' << '=' << (value ? 't' : 'f'); },
                   [this, field](double value) { m_fields << ',' << field.name << '=' << value; } },
        field.value);
    return *this;
}

auto database::entry::commit(std::int_fast64_t timestamp) -> bool
{
    if (m_fields.str().empty()) {
        return false;
    }
    m_tags << ' '
           << m_fields.str().substr(1)
           << ' ' << timestamp;
    return m_link.send_string(m_tags.str());
}

database::database(Config::Influx config)
    : m_config { std::move(config) }
{
}

database::database() = default;

database::~database() = default;

auto database::measurement(const std::string& measurement) -> entry
{
    return entry { measurement, *this };
}

auto database::send_string(const std::string& query) const -> bool
{
    CURL* curl { curl_easy_init() };

    if (curl != nullptr) {
        scope_guard guard { [&curl] { curl_easy_cleanup(curl); } };

        std::ostringstream url {};
        url
            << m_config.host
            << "/write?db="
            << m_config.database
            << "&u=" << m_config.login.username
            << "&p=" << m_config.login.password
            << "&epoch=ms";

        curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());
        curl_easy_setopt(curl, CURLOPT_PORT, s_port);

        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, query.c_str());

        CURLcode res { curl_easy_perform(curl) };

        long http_code { 0 };
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        if (res != CURLE_OK) {
            log::warning() << "Couldn't write to database: " + std::to_string(http_code) + ": " + std::string { curl_easy_strerror(res) };
            return false;
        }
        if ((http_code / 100) != 2) {
            log::warning() << "Couldn't write to database: " + std::to_string(http_code);
            return false;
        }
    }
    return true;
}

} // namespace muonpi
