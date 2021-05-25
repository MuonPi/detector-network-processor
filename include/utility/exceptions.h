#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include <boost/stacktrace.hpp>
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>

namespace muonpi::error {

class mqtt_could_not_subscribe : std::runtime_error {
public:
    mqtt_could_not_subscribe(const std::string& name, const std::string& reason)
        : std::runtime_error { "Could not subscribe to mqtt topic '" + name + "': " + reason }
    {
    }
};

class mqtt_could_not_publish : std::runtime_error {
public:
    mqtt_could_not_publish(const std::string& name, const std::string& reason)
        : std::runtime_error { "Could not publish mqtt topic '" + name + "': " + reason }
    {
    }
};

class config_option_not_found : std::runtime_error {
public:
    config_option_not_found(const std::string& name)
        : std::runtime_error { "Could not find configuration option '" + name + "'" }
    {
    }
};

void terminate_handler();

}

namespace boost {
void assertion_failed_msg(char const* expr, char const* msg, char const* function, char const* /*file*/, long /*line*/);

void assertion_failed(char const* expr, char const* function, char const* file, long line);
} // namespace boost

#endif // EXCEPTIONS_H
