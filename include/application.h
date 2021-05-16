#ifndef APPLICATION_H
#define APPLICATION_H

#include "supervision/state.h"

#include <csignal>
#include <functional>

namespace muonpi {

class application {
public:
    [[nodiscard]] static auto setup(int argc, const char* argv[]) -> bool;
    [[nodiscard]] static auto run() -> int;

    static void shutdown(int exit_code);

    static void signal_handler(int signal);

private:
    [[nodiscard]] auto priv_run() -> int;

    std::unique_ptr<supervision::state> m_supervisor;

    const static std::unique_ptr<application> s_singleton;

    friend void wrapper_signal_handler(int signal);
};

}

#endif // APPLICATION_H
