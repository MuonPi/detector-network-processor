#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include "defaults.h"

#include <muonpi/log.h>
#include <boost/program_options.hpp>

#include <fstream>

namespace muonpi {
class config {
public:
    class initialisation {
    public:
        [[nodiscard]] auto operator()(std::string name, std::string description) -> initialisation&
        {
            m_init(name.c_str(), description.c_str());
            return *this;
        }

        template <typename T>
        [[nodiscard]] auto operator()(std::string name, T value) -> initialisation&
        {
            m_init(name.c_str(), value);
            return *this;
        }

        template <typename T>
        [[nodiscard]] auto operator()(std::string name, T value, std::string description) -> initialisation&
        {
            m_init(name.c_str(), value, description.c_str());
            return *this;
        }

        void commit(int argc, const char* argv[])
        {
            boost::program_options::store(boost::program_options::parse_command_line(argc, argv, m_desc), m_options);
        }

        void commit(std::string filename)
        {
            std::ifstream ifs { filename };
            if (ifs) {
                boost::program_options::store(boost::program_options::parse_config_file(ifs, m_desc), m_options);
            } else {
                log::error() << "Could not open configuration file '" << filename << "'.";
                throw std::runtime_error("Could not open configuration file '" + filename + "'.");
            }
            s_instances--;
            if (s_instances == 0) {
                boost::program_options::notify(m_options);
            }
        }

    private:
        initialisation(std::string name, boost::program_options::variables_map& options)
            : m_options { options }
            , m_desc { name }
            , m_init { m_desc.add_options() }
        {
            s_instances++;
        }

        friend class config;

        boost::program_options::variables_map& m_options;
        boost::program_options::options_description m_desc;
        boost::program_options::options_description_easy_init m_init;
        static std::size_t s_instances;
    };

    [[nodiscard]] static auto setup(std::string description) -> initialisation
    {
        return initialisation{description, singleton()->m_options};
    }

    [[nodiscard]] static auto singleton() -> const std::unique_ptr<config>&;

    template <typename T>
    [[nodiscard]] static inline auto get(std::string name) -> T;

    [[nodiscard]] static inline auto is_set(std::string name) -> bool;

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

template <typename T>
auto config::get(std::string name) -> T
{
    if (!is_set(name)) {
        log::error()<<"Option '" << name << "' not set.";
        throw std::runtime_error("Option '" + name + "' not set.");
    }
    return singleton()->m_options[std::move(name)].as<T>();
}

auto config::is_set(std::string name) -> bool
{
    return singleton()->m_options.find(std::move(name)) != singleton()->m_options.end();
}
}

#endif // CONFIGURATION_H
