#ifndef REST_SERVICE_H
#define REST_SERVICE_H

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
#include <map>

namespace MuonPi::rest {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;

using request = http::request<http::string_body>;
using response = http::response<http::string_body>;
using tcp = net::ip::tcp;


struct handler
{
    const std::string path{};
    const std::function<bool (const request& req, std::string_view username, std::string_view password)> authenticate {};
    const std::function<response (const request& req)> handle {};
    const std::map<std::string, handler> children {};
};

class service : public std::enable_shared_from_this<service>
{
public:
    service(
            net::io_context& ioc,
            ssl::context& ctx,
            tcp::endpoint endpoint,
            std::shared_ptr<std::string const> const& doc_root);

    void run();

    [[nodiscard]] auto handle(request req) const -> response;

    void add_handler(std::string, handler han);

private:
    void do_accept();

    void on_accept(beast::error_code error_code, tcp::socket socket);


    std::map<std::string, handler> m_handler {};
};

}

#endif // REST_SERVICE_H
