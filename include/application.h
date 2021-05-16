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
    std::unique_ptr<supervision::state> m_supervisor { nullptr };

    static std::function<void(int)> s_shutdown_handler;

    friend void wrapper_signal_handler(int signal);
};

}

#endif // APPLICATION_H
