#include "utility/rest_service.h"
#include "utility/log.h"

namespace MuonPi::rest {

void fail(beast::error_code ec, const std::string& what);

class session : public std::enable_shared_from_this<session>
{
public:
    explicit session(tcp::socket&& socket, ssl::context& ctx, service& ser);

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

    beast::ssl_stream<beast::tcp_stream> m_stream{};
    beast::flat_buffer m_buffer {};
    http::request<http::string_body> m_req {};
    std::shared_ptr<void> m_res {};
    service& m_service;
};


void fail(beast::error_code ec, const std::string& what)
{
    if(ec == net::ssl::error::stream_truncated) {
        return;
    }

    Log::warning() << what + ": " + ec.message();
}


session::session(tcp::socket&& socket, ssl::context& ctx, service& ser)
    : m_stream{std::move(socket), ctx}
    , m_service { ser }
{}

void session::run()
{
    net::dispatch(
               m_stream.get_executor(),
               beast::bind_front_handler<decltype (session::on_run()), std::shared_ptr<session>>(
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

    send(m_service.handle(std::move(m_req)));
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
    // Set the timeout.
    beast::get_lowest_layer(m_stream).expires_after(std::chrono::seconds(30));

    // Perform the SSL shutdown
    m_stream.async_shutdown(
        beast::bind_front_handler(
            &session::on_shutdown,
            shared_from_this()));
}

void session::on_shutdown(beast::error_code ec)
{
    if(ec) {
        fail(ec, "shutdown");
        return;
    }
}

void session::send(http::message<false, http::string_body>&& message)
{

    // The lifetime of the message has to extend
    // for the duration of the async operation so
    // we use a shared_ptr to manage it.
    auto sp = std::make_shared<http::message<false, http::string_body>>(std::move(message));

    // Store a type-erased version of the shared
    // pointer in the class to keep it alive.
    m_res = sp;

    // Write the response
    http::async_write(
        m_stream,
        *sp,
        beast::bind_front_handler(
            &session::on_write,
            shared_from_this(),
            sp->need_eof()));
}
}
