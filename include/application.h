#ifndef APPLICATION_H
#define APPLICATION_H

#include "supervision/state.h"

#include <csignal>
#include <functional>

#include <boost/program_options.hpp>

namespace muonpi {

class application {
public:
    [[nodiscard]] auto setup(int argc, const char* argv[]) -> bool;
    [[nodiscard]] auto run() -> int;

    void signal_handler(int signal);

private:
    boost::program_options::variables_map m_options {};

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

    std::unique_ptr<supervision::state> m_supervisor { nullptr };

    static std::function<void(int)> s_shutdown_handler;

    friend void wrapper_signal_handler(int signal);
};

}

#endif // APPLICATION_H
