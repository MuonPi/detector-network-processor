#ifndef APPLICATION_H
#define APPLICATION_H

#include "utility/configuration.h"
#include "utility/parameters.h"

#include "supervision/state.h"

#include <csignal>
#include <functional>

namespace muonpi {

class application {
public:
    [[nodiscard]] auto setup(std::vector<std::string> arguments) -> bool;
    [[nodiscard]] auto run() -> int;

    void signal_handler(int signal);

private:
    [[nodiscard]] static auto credentials(std::string filename, bool encrypted = false) -> configuration;
    [[nodiscard]] static auto config(std::string filename) -> configuration;
    [[nodiscard]] static auto parameter() -> parameters;

    parameters m_parameters { parameter() };


    std::unique_ptr<supervision::state> m_supervisor { nullptr };


    static std::function<void(int)> s_shutdown_handler;

    friend void wrapper_signal_handler(int signal);
};

}

#endif // APPLICATION_H
