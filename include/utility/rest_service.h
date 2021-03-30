#ifndef REST_SERVICE_H
#define REST_SERVICE_H

#include "defaults.h"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/config.hpp>


#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <string_view>
#include <queue>

namespace MuonPi::rest {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;

using request = http::request<http::string_body>;
using response = http::response<http::string_body>;
using tcp = net::ip::tcp;

template <http::status status>
[[nodiscard]] inline auto http_response(request& req, std::string why) -> response
{
    http::response<http::string_body> res{status, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.keep_alive(req.keep_alive());
    res.body() = std::move(why);
    res.prepare_payload();
    return res;
}

struct handler
{
    std::function<bool (std::string_view path)> matches {};
    std::function<bool (const request& req, std::string_view username, std::string_view password)> authenticate {};
    std::function<response (const request& req, std::queue<std::string> path)> handle {};
    std::vector<handler> children {};
    bool requires_auth { false };
};

class listener;

class service
{
public:
    service(Config::Rest rest_config);

    void add_handler(handler han);

private:
    std::string m_address {};
    std::uint16_t m_port {};

    std::shared_ptr<listener> m_listener { nullptr };
};

}

#endif // REST_SERVICE_H
