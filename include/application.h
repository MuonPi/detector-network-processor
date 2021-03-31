#ifndef APPLICATION_H
#define APPLICATION_H

#include "utility/configuration.h"
#include "utility/parameters.h"

#include "sink/base.h"

#include "link/database.h"

#include "messages/detectorlog.h"

#include "supervision/state.h"


#include <csignal>
#include <functional>

namespace muonpi {

class application
{
public:
    application();
    ~application();

    [[nodiscard]] auto setup(std::vector<std::string> arguments) -> bool;
    [[nodiscard]] auto run() -> int;

    void signal_handler(int signal);

private:
    [[nodiscard]] static auto credentials(std::string filename, bool encrypted = false) -> configuration;
    [[nodiscard]] static auto config(std::string filename) -> configuration;
    [[nodiscard]] static auto parameter() -> parameters;

    parameters m_parameters { parameter() };


    link::database* m_db_link { nullptr };

    muonpi::sink::base<event_t>* m_event_sink { nullptr };
    muonpi::sink::base<cluster_log_t>* m_clusterlog_sink { nullptr };
    muonpi::sink::base<detetor_summary_t>* m_detectorsummary_sink { nullptr };

    muonpi::sink::base<event_t>* m_broadcast_event_sink { nullptr };

    muonpi::sink::base<detector_log_t>* m_detectorlog_sink { nullptr };

    muonpi::sink::base<event_t>* m_ascii_event_sink { nullptr };
    muonpi::sink::base<cluster_log_t>* m_ascii_clusterlog_sink { nullptr };
    muonpi::sink::base<detetor_summary_t>* m_ascii_detectorsummary_sink { nullptr };

    state_supervisor* m_supervisor { nullptr };

    static std::function<void(int)> s_shutdown_handler;

    friend void wrapper_signal_handler(int signal);
};

}

#endif // APPLICATION_H
