#ifndef APPLICATION_H
#define APPLICATION_H

#include "utility/configuration.h"
#include "utility/parameters.h"

#include "sink/base.h"

#include "link/database.h"
#include "link/mqtt.h"

#include "messages/detectorlog.h"

#include "supervision/state.h"

#include "messages/trigger.h"

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

    std::unique_ptr<link::database> m_db_link { nullptr };
    std::unique_ptr<link::mqtt> m_sink_mqtt_link { nullptr };

    template <typename T>
    using sink_ptr = std::unique_ptr<sink::base<T>>;

    sink_ptr<trigger::detector> m_trigger_sink { nullptr };

    sink_ptr<event_t> m_event_sink { nullptr };
    sink_ptr<cluster_log_t> m_clusterlog_sink { nullptr };
    sink_ptr<detector_summary_t> m_detectorsummary_sink { nullptr };

    sink_ptr<event_t> m_broadcast_event_sink { nullptr };

    sink_ptr<detector_log_t> m_detectorlog_sink { nullptr };

    sink_ptr<event_t> m_ascii_event_sink { nullptr };
    sink_ptr<cluster_log_t> m_ascii_clusterlog_sink { nullptr };
    sink_ptr<detector_summary_t> m_ascii_detectorsummary_sink { nullptr };

    std::unique_ptr<supervision::state> m_supervisor { nullptr };


    static std::function<void(int)> s_shutdown_handler;

    friend void wrapper_signal_handler(int signal);
};

}

#endif // APPLICATION_H
