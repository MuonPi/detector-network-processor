#ifndef REST_SERVICE_H
#define REST_SERVICE_H

#include "defaults.h"
#include "threadrunner.h"

#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/config.hpp>

#include <queue>
#include <string>
#include <string_view>
#include <vector>

namespace muonpi::rest {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;

using request_type = http::request<http::string_body>;
using response_type = http::response<http::string_body>;
using tcp = net::ip::tcp;

template <http::status status>
[[nodiscard]] inline auto http_response(request_type& req, std::string why) -> response_type
{
    response_type res { status, req.version() };
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.keep_alive(req.keep_alive());
    res.body() = std::move(why);
    res.prepare_payload();
    return res;
}

struct request {
    request_type& req;

    template <http::status status>
    [[nodiscard]] inline auto response(std::string why) -> response_type
    {
        return http_response<status>(req, why);
    }
};

struct handler {
    std::function<bool(std::string_view path)> matches {};
    std::function<bool(request req, std::string_view username, std::string_view password)> authenticate {};
    std::function<response_type(request req, const std::queue<std::string>& path)> handle {};
    std::vector<handler> children {};
    bool requires_auth { false };
};

class service_handler {
public:
    [[nodiscard]] auto get_handler() -> handler;

protected:
    void set_handler(handler h);

private:
    handler m_handler {};
};

class service : public thread_runner {
public:
    service(Config::Rest rest_config);

    void add_handler(service_handler* han);

protected:
    [[nodiscard]] auto custom_run() -> int override;

    void do_accept();

    void on_stop() override;

private:
    [[nodiscard]] auto handle(request_type req) const -> response_type;

    [[nodiscard]] auto handle(request_type req, std::queue<std::string> path, const std::vector<handler>& handlers) const -> response_type;

    std::vector<handler> m_handler {};

    net::io_context m_ioc { 1 };
#ifndef CLUSTER_DISABLE_SSL
    ssl::context m_ctx { ssl::context::tlsv12 };
#endif
    tcp::acceptor m_acceptor { m_ioc };
    tcp::endpoint m_endpoint;
    Config::Rest m_rest_conf;
};

}

#endif // REST_SERVICE_H
