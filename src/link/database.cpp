#include "link/database.h"

#include "utility/log.h"

#include <type_traits>
#include <utility>
#include <variant>

#include <curl/curl.h>

namespace MuonPi::Link {
Database::Entry::Entry(const std::string& measurement, Database& link)
    : m_link { link }
{
    m_stream << measurement;
}

auto Database::Entry::operator<<(const Influx::Tag& tag) -> Entry&
{
    m_stream << ',' << tag.name << '=' << tag.field;
    return *this;
}

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

auto Database::Entry::operator<<(const Influx::Field& field) -> Entry&
{
    std::visit(overloaded {
                   [this, field](const std::string& value) {m_stream<<(m_has_field?',':' ')<<field.name<<"=\""<<value<<'"'; m_has_field = true; },
                   [this, field](std::int_fast64_t value) {m_stream<<(m_has_field?',':' ')<<field.name<<'='<<value<<'i'; m_has_field = true; },
                   [this, field](std::size_t value) {m_stream<<(m_has_field?',':' ')<<field.name<<'='<<value<<'i'; m_has_field = true; },
                   [this, field](std::uint8_t value) {m_stream<<(m_has_field?',':' ')<<field.name<<'='<<static_cast<std::uint16_t>(value)<<'i'; m_has_field = true; },
                   [this, field](std::uint16_t value) {m_stream<<(m_has_field?',':' ')<<field.name<<'='<<value<<'i'; m_has_field = true; },
                   [this, field](std::uint32_t value) {m_stream<<(m_has_field?',':' ')<<field.name<<'='<<value<<'i'; m_has_field = true; },
                   //                   [this, field](std::uint64_t value){m_stream<<(m_has_field?',':' ')<<field.name<<'='<<value<<'i'; m_has_field = true;},
                   [this, field](bool value) {m_stream<<field.name<<(m_has_field?',':' ')<<'='<<(value?'t':'f'); m_has_field = true; },
                   [this, field](double value) {m_stream<<(m_has_field?',':' ')<<field.name<<'='<<value; m_has_field = true; } },
        field.value);
    return *this;
}

auto Database::Entry::operator<<(std::int_fast64_t timestamp) -> bool
{
    m_stream << ' ' << timestamp;
    return m_link.send_string(m_stream.str());
}

Database::Database(Config::Influx config)
    : m_config { std::move(config) }
{
}

Database::Database() = default;

Database::~Database() = default;

auto Database::measurement(const std::string& measurement) -> Entry
{
    return Entry { measurement, *this };
}

auto Database::send_string(const std::string& query) -> bool
{
    CURL* curl { curl_easy_init() };

    if (curl != nullptr) {
        class CurlGuard {
        public:
            explicit CurlGuard(CURL* curl)
                : m_curl { curl }
            {
            }
            ~CurlGuard() { curl_easy_cleanup(m_curl); }

        private:
            CURL* m_curl { nullptr };
        } curl_guard { curl };

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
            Log::warning() << "Couldn't write to Database: " + std::to_string(http_code) + ": " + std::string { curl_easy_strerror(res) };
            return false;
        }
        if ((http_code / 100) != 2) {
            Log::warning() << "Couldn't write to Database: " + std::to_string(http_code);
            return false;
        }
    }
    return true;
}

} // namespace MuonPi
