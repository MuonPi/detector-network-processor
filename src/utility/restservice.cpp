#include "utility/restservice.h"
#include "utility/base64.h"
#include "utility/log.h"

#include <sstream>

namespace muonpi::rest {

auto service_handler::get_handler() -> handler
{
    return m_handler;
}

void service_handler::set_handler(handler h)
{
    m_handler = std::move(h);
}

service::service(Config::Rest rest_config)
    : thread_runner("REST")
    , m_endpoint { net::ip::make_address(rest_config.address), static_cast<std::uint16_t>(rest_config.port) }
    , m_rest_conf { std::move(rest_config) }
{
    m_ctx.set_options(
        boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2);

    m_ctx.use_private_key_file(m_rest_conf.privkey, ssl::context::file_format::pem);
    m_ctx.use_certificate_file(m_rest_conf.cert, ssl::context::file_format::pem);
    m_ctx.use_certificate_chain_file(m_rest_conf.fullchain);

    beast::error_code ec;

    m_acceptor.open(m_endpoint.protocol(), ec);
    if (ec) {
        fail(ec, "open");
        return;
    }

    m_acceptor.set_option(net::socket_base::reuse_address(true), ec);
    if (ec) {
        fail(ec, "set_option");
        return;
    }

    m_acceptor.bind(m_endpoint, ec);
    if (ec) {
        fail(ec, "bind");
        return;
    }

    m_acceptor.listen(net::socket_base::max_listen_connections, ec);
    if (ec) {
        fail(ec, "listen");
        return;
    }

    start();
}

void service::add_handler(service_handler* han)
{
    m_handler.emplace_back(han->get_handler());
}

auto service::step() -> int
{
    tcp::socket socket { m_ioc };

    m_acceptor.accept(socket);

    auto f { std::async(std::launch::async, [&] { session(socket); }) };
    return 0;
}

void service::session(tcp::socket& socket)
{
    beast::error_code ec;

    beast::ssl_stream<tcp::socket&> stream { socket, m_ctx };

    stream.handshake(ssl::stream_base::server, ec);

    if (ec) {
        fail(ec, "handshake");
        return;
    }

    beast::flat_buffer buffer;

    for (bool close { false }; !close;) {
        http::request<http::string_body> req;
        http::read(stream, buffer, req, ec);
        if (ec == http::error::end_of_stream) {
            break;
        }

        if (ec) {
            fail(ec, "read");
            return;
        }

        auto res { handle(std::move(req)) };

        close = res.need_eof();

        http::serializer<false, http::string_body> sr { res };

        http::write(stream, sr, ec);

        if (ec) {
            fail(ec, "write");
            return;
        }
    }

    stream.shutdown(ec);

    if (ec) {
        fail(ec, "shutdown");
    }
}

auto service::handle(request_type req) const -> response_type
{
    if (req.target().empty() || req.target()[0] != '/' || (req.target().find("..") != beast::string_view::npos)) {
        return http_response<http::status::bad_request>(req, "Illegal request-target");
    }
    if (m_handler.empty()) {
        return http_response<http::status::service_unavailable>(req, "No handler installed");
    }

    std::queue<std::string> path {};
    {
        std::istringstream stream { req.target().to_string() };
        for (std::string part; std::getline(stream, part, '/');) {
            path.emplace(part);
        }
    }

    return handle(std::move(req), std::move(path), m_handler);
}

auto service::handle(request_type req, std::queue<std::string> path, const std::vector<handler>& handlers) const -> response_type
{
    while (!path.empty() && path.front().empty()) {
        path.pop();
    }

    if (path.empty()) {
        return http_response<http::status::bad_request>(req, "Illegal request-target");
    }

    auto it = handlers.begin();
    for (; it != handlers.end(); it++) {
        if ((*it).matches(path.front())) {
            continue;
        }
    }
    if (it == handlers.end()) {
        return http_response<http::status::bad_request>(req, "Illegal request-target");
    }

    const handler& hand { *it };

    path.pop();

    if (hand.requires_auth) {
        std::string auth { req[http::field::authorization] };

        if (auth.empty()) {
            return http_response<http::status::unauthorized>(req, "Unauthorised");
        }
        constexpr std::size_t header_length { 6 };

        auth = base64::decode(auth.substr(header_length));

        auto delimiter = auth.find_first_of(':');
        auto username = auth.substr(0, delimiter);
        auto password = auth.substr(delimiter + 1);

        if (!hand.authenticate(request { req }, username, password)) {
            return http_response<http::status::unauthorized>(req, "Unauthorised");
        }
    }

    if (hand.children.empty() || path.empty()) {
        return hand.handle(request { req }, path);
    }

    return handle(std::move(req), std::move(path), hand.children);
}

void service::fail(beast::error_code ec, const std::string& what)
{
    if (ec == net::ssl::error::stream_truncated) {
        return;
    }

    log::warning() << what + ": " + ec.message();
}

}
