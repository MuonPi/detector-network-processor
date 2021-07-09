#include "application.h"

#include "defaults.h"

#include "utility/log.h"

#include "analysis/coincidencefilter.h"
#include "analysis/stationcoincidence.h"
#include "supervision/station.h"

#include "messages/detectorlog.h"
#include "messages/trigger.h"

#include "link/database.h"
#include "link/mqtt.h"

#include "source/mqtt.h"

#include "sink/ascii.h"
#include "sink/base.h"
#include "sink/database.h"
#include "sink/mqtt.h"

#include "utility/configuration.h"
#include "utility/exceptions.h"

#include <exception>
#include <memory>

namespace muonpi {

const std::unique_ptr<application> application::s_singleton { std::make_unique<application>() };

void wrapper_signal_handler(int signal)
{
    application::signal_handler(signal);
}

auto application::setup(int argc, const char* argv[]) -> bool
{
    std::set_terminate(error::terminate_handler);

    auto now {std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())};

    log::info() << "detector-network-processor " << Version::string() << "\n" << std::ctime(&now);

    return config::singleton()->setup(argc, argv);
}

template <typename T>
using sink_ptr = std::unique_ptr<sink::base<T>>;

auto application::run() -> int
{
    return s_singleton->priv_run();
}

void application::shutdown(int exit_code)
{
    s_singleton->m_supervisor->stop(exit_code);
}

auto application::priv_run() -> int
{
    std::unique_ptr<link::database> db_link { nullptr };
    std::unique_ptr<link::mqtt> sink_mqtt_link { nullptr };
    std::unique_ptr<station_coincidence> stationcoincidence { nullptr };

    sink_ptr<trigger::detector> mqtt_trigger_sink { nullptr };
    sink_ptr<trigger::detector> trigger_sink { nullptr };

    sink_ptr<event_t> event_sink { nullptr };
    sink_ptr<cluster_log_t> clusterlog_sink { nullptr };
    sink_ptr<detector_summary_t> detectorsummary_sink { nullptr };

    sink_ptr<event_t> broadcast_event_sink { nullptr };

    sink_ptr<detector_log_t> detectorlog_sink { nullptr };

    sink_ptr<event_t> ascii_event_sink { nullptr };
    sink_ptr<cluster_log_t> ascii_clusterlog_sink { nullptr };
    sink_ptr<detector_summary_t> ascii_detectorsummary_sink { nullptr };
    sink_ptr<trigger::detector> ascii_trigger_sink { nullptr };

    link::mqtt source_mqtt_link { config::singleton()->source_mqtt, config::singleton()->meta.station + "_sink", "muon::mqtt::so" };
    if (!source_mqtt_link.wait_for(link::mqtt::Status::Connected)) {
        return -1;
    }

    if (!config::singleton()->option_set("offline")) {
        sink_mqtt_link = std::make_unique<link::mqtt>(config::singleton()->sink_mqtt, config::singleton()->meta.station + "_sink", "muon::mqtt:si");
        if (!sink_mqtt_link->wait_for(link::mqtt::Status::Connected)) {
            return -1;
        }
    }

    sink::collection<event_t> collection_event_sink { "muon::sink::e" };
    sink::collection<cluster_log_t> collection_clusterlog_sink { "muon::sink::c" };
    sink::collection<detector_summary_t> collection_detectorsummary_sink { "muon::sink::d" };
    sink::collection<trigger::detector> collection_trigger_sink { "muon::sink::t" };
    sink::collection<detector_log_t> collection_detectorlog_sink { "muon::sink::l" };

    if (config::singleton()->option_set("debug")) {
        ascii_event_sink = std::make_unique<sink::ascii<event_t>>(std::cout);
        ascii_clusterlog_sink = std::make_unique<sink::ascii<cluster_log_t>>(std::cout);
        ascii_detectorsummary_sink = std::make_unique<sink::ascii<detector_summary_t>>(std::cout);
        ascii_trigger_sink = std::make_unique<sink::ascii<trigger::detector>>(std::cout);

        collection_event_sink.emplace(*ascii_event_sink);
        collection_clusterlog_sink.emplace(*ascii_clusterlog_sink);
        collection_detectorsummary_sink.emplace(*ascii_detectorsummary_sink);
        collection_trigger_sink.emplace(*ascii_trigger_sink);
    }

    if (!config::singleton()->option_set("offline")) {
        mqtt_trigger_sink = std::make_unique<sink::mqtt<trigger::detector>>(sink_mqtt_link->publish("muonpi/trigger"));
        collection_trigger_sink.emplace(*mqtt_trigger_sink);

        if (!config::singleton()->meta.local_cluster) {
            db_link = std::make_unique<link::database>(config::singleton()->influx);

            event_sink = std::make_unique<sink::database<event_t>>(*db_link);
            clusterlog_sink = std::make_unique<sink::database<cluster_log_t>>(*db_link);
            detectorsummary_sink = std::make_unique<sink::database<detector_summary_t>>(*db_link);
            broadcast_event_sink = std::make_unique<sink::mqtt<event_t>>(sink_mqtt_link->publish("muonpi/events"));
            detectorlog_sink = std::make_unique<sink::database<detector_log_t>>(*db_link);
            trigger_sink = std::make_unique<sink::database<trigger::detector>>(*db_link);

            collection_trigger_sink.emplace(*trigger_sink);
            collection_event_sink.emplace(*broadcast_event_sink);

        } else {
            event_sink = std::make_unique<sink::mqtt<event_t>>(sink_mqtt_link->publish("muonpi/l1data"), true);
            clusterlog_sink = std::make_unique<sink::mqtt<cluster_log_t>>(sink_mqtt_link->publish("muonpi/cluster"));
            detectorsummary_sink = std::make_unique<sink::mqtt<detector_summary_t>>(sink_mqtt_link->publish("muonpi/cluster"));
            detectorlog_sink = std::make_unique<sink::mqtt<detector_log_t>>(sink_mqtt_link->publish("muonpi/log/"));
        }
        collection_event_sink.emplace(*event_sink);
        collection_clusterlog_sink.emplace(*clusterlog_sink);
        collection_detectorsummary_sink.emplace(*detectorsummary_sink);
        collection_detectorlog_sink.emplace(*detectorlog_sink);
    }

    m_supervisor = std::make_unique<supervision::state>(collection_clusterlog_sink);
    coincidence_filter coincidencefilter { collection_event_sink, *m_supervisor };
    supervision::timebase timebasesupervisor { coincidencefilter, coincidencefilter };
    supervision::station stationsupervisor { collection_detectorsummary_sink, collection_trigger_sink, timebasesupervisor, timebasesupervisor, *m_supervisor };

    source::mqtt<event_t> event_source { stationsupervisor, source_mqtt_link.subscribe("muonpi/data/#") };
    source::mqtt<event_t> l1_source { stationsupervisor, source_mqtt_link.subscribe("muonpi/l1data/#") };
    source::mqtt<detector_info_t<location_t>> detector_location_source { stationsupervisor, source_mqtt_link.subscribe("muonpi/log/#") };

    source::mqtt<detector_log_t> detectorlog_source { collection_detectorlog_sink, source_mqtt_link.subscribe("muonpi/log/#") };

    if (config::singleton()->option_set("histogram")) {
        stationcoincidence = std::make_unique<station_coincidence>(config::singleton()->get_option<std::string>("histogram"), stationsupervisor);

        collection_event_sink.emplace(*stationcoincidence);
        collection_trigger_sink.emplace(*stationcoincidence);

        m_supervisor->add_thread(*stationcoincidence);
    }

    m_supervisor->add_thread(stationsupervisor);
    m_supervisor->add_thread(coincidencefilter);
    if (sink_mqtt_link != nullptr) {
        m_supervisor->add_thread(*sink_mqtt_link);
    }
    m_supervisor->add_thread(source_mqtt_link);
    m_supervisor->add_thread(collection_event_sink);
    m_supervisor->add_thread(collection_detectorsummary_sink);
    m_supervisor->add_thread(collection_clusterlog_sink);
    m_supervisor->add_thread(collection_trigger_sink);
    m_supervisor->add_thread(collection_detectorlog_sink);

    std::signal(SIGINT, wrapper_signal_handler);
    std::signal(SIGTERM, wrapper_signal_handler);
    std::signal(SIGHUP, wrapper_signal_handler);

    m_supervisor->start_synchronuos();

    return m_supervisor->wait();
}

void application::signal_handler(int signal)
{
    if ((signal == SIGINT) || (signal == SIGTERM) || (signal == SIGHUP)) {
        log::notice() << "Received signal: " << std::to_string(signal) << ". Exiting.";
        s_singleton->m_supervisor->stop(1);
    }
}

} // namespace muonpi
