#include "link/database.h"

#include "utility/log.h"
#include "utility/scopeguard.h"

#include <type_traits>
#include <utility>
#include <variant>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <cstdlib>
#include <iostream>
#include <string>

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
    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace net = boost::asio;
    using tcp = net::ip::tcp;

    std::ostringstream target {};
    target
        << "/write?db="
        << m_config.database
        << "&u=" << m_config.login.username
        << "&p=" << m_config.login.password
        << "&epoch=ms";

    const auto* const host { m_config.host.c_str() };
    auto const port { std::to_string(s_port) };
    const int version { 11 };

    net::io_context ioc;
    tcp::resolver resolver(ioc);
    beast::tcp_stream stream(ioc);
    auto const results = resolver.resolve(host, port);
    stream.connect(results);

    http::request<http::string_body> req { http::verb::post, target.str(), version };
    req.set(http::field::host, host);
    req.set(http::field::user_agent, "detector-network-processor");
    req.set(http::field::content_type, "application/x-www-form-urlencoded");
    req.set(http::field::accept, "*/*");
    req.set(http::field::content_length, std::to_string(query.size()));
    req.body() = query;

    http::write(stream, req);
    beast::flat_buffer buffer;
    http::response<http::string_body> res;
    http::read(stream, buffer, res);

    if (res.result() != http::status::no_content) {
        log::warning() << "Couldn't write to database: " << std::to_string(static_cast<unsigned>(res.result())) << ": " << res.body();
        return false;
    }
    beast::error_code ec;
    stream.socket().shutdown(tcp::socket::shutdown_both, ec);
    if (ec && (ec != net::error::eof)) { // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
        log::warning() << "Could not write to database: " << ec.message();
        return false;
    }
    return true;
}

} // namespace muonpi::link
