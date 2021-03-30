#include "utility/rest_service.h"
#include "utility/log.h"
#include "utility/base64.h"


#include <sstream>


namespace MuonPi::rest {

void fail(beast::error_code ec, const std::string& what);


class listener : public std::enable_shared_from_this<listener>
{
public:
    listener(Config::Rest rest_config);

    void run();

    [[nodiscard]] auto handle(request req) const -> response;

    void add_handler(handler han);

private:
    void do_accept();

    void on_accept(beast::error_code error_code, tcp::socket socket);

    [[nodiscard]] auto handle(request req, std::queue<std::string> path, const std::vector<handler>& handlers) const -> response;

    std::vector<handler> m_handler {};

    net::io_context m_ioc { 1 };
    ssl::context m_ctx { ssl::context::tlsv12 };
    tcp::acceptor m_acceptor { m_ioc };
    tcp::endpoint m_endpoint;
    Config::Rest m_rest_conf;
};

class session : public std::enable_shared_from_this<session>
{
public:
    explicit session(tcp::socket&& socket, ssl::context& ctx, listener& l);

    void run();

    void on_run();

    void on_handshake(beast::error_code error_code);

    void do_read();

    void on_read(beast::error_code error_code, std::size_t bytes_transferred);

    void on_write(bool close, beast::error_code error_code, std::size_t bytes_transferred);

    void do_close();

    void on_shutdown(beast::error_code error_code);


private:
    void send(http::message<false, http::string_body>&& message);

    beast::ssl_stream<beast::tcp_stream> m_stream;
    beast::flat_buffer m_buffer {};
    http::request<http::string_body> m_req {};
    std::shared_ptr<void> m_res {};
    listener& m_listener;
};


void fail(beast::error_code ec, const std::string& what)
{
    if(ec == net::ssl::error::stream_truncated) {
        return;
    }

    Log::warning() << what + ": " + ec.message();
}


session::session(tcp::socket&& socket, ssl::context& ctx, listener& l)
    : m_stream{std::move(socket), ctx}
    , m_listener { l }
{}

void session::run()
{
    net::dispatch(
               m_stream.get_executor(),
               beast::bind_front_handler(
                   &session::on_run,
                   shared_from_this()));
}

void session::on_run()
{
    // Set the timeout.
    beast::get_lowest_layer(m_stream).expires_after(
        std::chrono::seconds(30));

    // Perform the SSL handshake
    m_stream.async_handshake(
        ssl::stream_base::server,
        beast::bind_front_handler(
            &session::on_handshake,
            shared_from_this()));
}

void session::on_handshake(beast::error_code ec)
{
    if(ec) {
        fail(ec, "handshake");
        return;
    }

    do_read();
}

void session::do_read()
{
    m_req = {};

    beast::get_lowest_layer(m_stream).expires_after(std::chrono::seconds(30));

    http::async_read(m_stream, m_buffer, m_req,
        beast::bind_front_handler(
            &session::on_read,
            shared_from_this()));
}

void session::on_read(
    beast::error_code ec,
    std::size_t bytes_transferred)
{
    boost::ignore_unused(bytes_transferred);

    if(ec == http::error::end_of_stream) {
        do_close();
        return;
    }

    if(ec) {
        fail(ec, "read");
        return;
    }

    send(m_listener.handle(std::move(m_req)));
}

void session::on_write(
    bool close,
    beast::error_code ec,
    std::size_t bytes_transferred)
{
    boost::ignore_unused(bytes_transferred);

    if(ec) {
        fail(ec, "write");
        return;
    }

    if(close)
    {
        do_close();
        return;
    }

    m_res = nullptr;

    do_read();
}

void session::do_close()
{
    beast::get_lowest_layer(m_stream).expires_after(std::chrono::seconds(30));

    m_stream.async_shutdown(
        beast::bind_front_handler(
            &session::on_shutdown,
            shared_from_this()));
}

void session::on_shutdown(beast::error_code ec)
{
    if(ec) {
        fail(ec, "shutdown");
    }
}

void session::send(http::message<false, http::string_body>&& message)
{
    auto sp = std::make_shared<http::message<false, http::string_body>>(std::move(message));

    m_res = sp;

    http::async_write(
        m_stream,
        *sp,
        beast::bind_front_handler(
            &session::on_write,
            shared_from_this(),
            sp->need_eof()));
}

listener::listener(Config::Rest rest_config)
    : m_endpoint{net::ip::make_address(rest_config.address), static_cast<std::uint16_t>(rest_config.port) }
    , m_rest_conf { std::move(rest_config) }
{
    m_ctx.set_options(
        boost::asio::ssl::context::default_workarounds |
        boost::asio::ssl::context::no_sslv2);

    m_ctx.use_private_key_file(m_rest_conf.privkey, ssl::context::file_format::pem);
    m_ctx.use_certificate_file(m_rest_conf.cert, ssl::context::file_format::pem);
    m_ctx.use_certificate_chain_file(m_rest_conf.fullchain);

    beast::error_code ec;

     // Open the acceptor
     m_acceptor.open(m_endpoint.protocol(), ec);
     if(ec)
     {
         fail(ec, "open");
         return;
     }

     // Allow address reuse
     m_acceptor.set_option(net::socket_base::reuse_address(true), ec);
     if(ec)
     {
         fail(ec, "set_option");
         return;
     }

     // Bind to the server address
     m_acceptor.bind(m_endpoint, ec);
     if(ec)
     {
         fail(ec, "bind");
         return;
     }

     // Start listening for connections
     m_acceptor.listen(
         net::socket_base::max_listen_connections, ec);
     if(ec)
     {
         fail(ec, "listen");
         return;
     }
}

void listener::run()
{
    do_accept();
}

void listener::do_accept()
{
    m_acceptor.async_accept(
         net::make_strand(m_ioc),
         beast::bind_front_handler(
             &listener::on_accept,
             shared_from_this()));
}

auto listener::handle(request req) const -> response
{
    if( req.target().empty() || req.target()[0] != '/' || (req.target().find("..") != beast::string_view::npos)) {
           return http_response<http::status::bad_request>(req, "Illegal request-target");
    }
    if (m_handler.empty()) {
       return http_response<http::status::service_unavailable>(req, "No handler installed");
    }

    std::queue<std::string> path {};
    {
        std::istringstream stream { req.target().to_string() };
        for (std::string part; std::getline(stream, part, '/'); ) {
            path.emplace(part);
        }
    }

    return handle(std::move(req), std::move(path), m_handler);
}

auto listener::handle(request req, std::queue<std::string> path, const std::vector<handler>& handlers) const -> response
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
        } else {
            auth = base64::decode(auth.substr(6));

            auto delimiter = auth.find_first_of(':');
            auto username = auth.substr(0, delimiter);
            auto password = auth.substr(delimiter + 1);

            if (!hand.authenticate(req, username, password)) {
                return http_response<http::status::unauthorized>(req, "Unauthorised");
            }
        }
    }

    if (hand.children.empty() || path.empty()) {
        return hand.handle(req, std::move(path));
    }

    return handle(std::move(req), std::move(path), hand.children);
}

void listener::add_handler(handler han)
{
    m_handler.emplace_back(std::move(han));
}

void listener::on_accept(beast::error_code ec, tcp::socket socket)
{
    if(ec)
    {
     fail(ec, "accept");
    }
    else
    {
     // Create the session and run it
     std::make_shared<session>(
         std::move(socket),
         m_ctx)->run();
    }

    do_accept();
}

service::service(Config::Rest rest_config)
    : m_listener{ std::make_shared<listener>(std::move(rest_config)) }
{
}

void service::add_handler(handler han)
{
    m_listener->add_handler(std::move(han));
}
}
