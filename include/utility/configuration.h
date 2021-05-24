#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include "defaults.h"

#include <boost/program_options.hpp>

namespace muonpi {
class config {
public:
    [[nodiscard]] static auto singleton() -> const std::unique_ptr<config>&;

    Config::Mqtt source_mqtt { Config::Default::mqtt };
    Config::Mqtt sink_mqtt { Config::Default::mqtt };
    Config::Influx influx { Config::Default::influx };
    Config::Ldap ldap { Config::Default::ldap };
    Config::Rest rest { Config::Default::rest };
    Config::Trigger trigger { Config::Default::trigger };
    Config::Interval interval { Config::Default::interval };
    Config::ConfigFiles files { Config::Default::files };
    Config::Meta meta { Config::Default::meta };

    [[nodiscard]] auto setup(int argc, const char* argv[]) -> bool;

    [[nodiscard]] inline auto option_set(std::string name) const -> bool
    {
        return m_options.find(std::move(name)) != m_options.end();
    }

    template <typename T>
    [[nodiscard]] inline auto get_option(std::string name) const -> T
    {
        return m_options[std::move(name)].as<T>();
    }

    template <typename T>
    auto check_option(std::string name, T& option) const -> bool
    {
        if (!option_set(name)) {
            return false;
        }
        option = get_option<T>(std::move(name));
        return true;
    }

private:
    static const std::unique_ptr<config> s_singleton;

    boost::program_options::variables_map m_options {};
};
}

#endif // CONFIGURATION_H
