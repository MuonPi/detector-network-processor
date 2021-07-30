#include "application.h"

#include "defaults.h"

#include "analysis/coincidencefilter.h"
#include "analysis/stationcoincidence.h"
#include "supervision/station.h"

#include "messages/detectorlog.h"
#include "messages/trigger.h"

#include "source/mqtt.h"

#include "sink/ascii.h"
#include "sink/database.h"
#include "sink/mqtt.h"

#include <muonpi/exceptions.h>
#include <muonpi/link/influx.h>
#include <muonpi/link/mqtt.h>
#include <muonpi/log.h>
#include <muonpi/sink/base.h>

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
    log::system::setup(muonpi::log::Level::Info, [&](int c) { shutdown(c); });

    std::set_terminate(error::terminate_handler);

    auto now { std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) };

    log::info() << "detector-network-processor " << Version::dnp::string() << "\n"
                << std::ctime(&now);

    auto optional = Config::setup(argc, argv);
    if (optional.has_value()) {
        s_singleton->m_config = optional.value();
        return true;
    }
    return false;
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
    std::unique_ptr<link::influx> db_link { nullptr };
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

    link::mqtt::configuration source_mqtt_config {};
    source_mqtt_config.host = m_config.get<std::string>("source_mqtt_host");
    source_mqtt_config.port = m_config.get<int>("source_mqtt_port");
    source_mqtt_config.login.username = m_config.get<std::string>("source_mqtt_user");
    source_mqtt_config.login.password = m_config.get<std::string>("source_mqtt_password");

    link::mqtt source_mqtt_link { source_mqtt_config, m_config.get<std::string>("station_id") + "_source", "muon::mqtt::so" };
    if (!source_mqtt_link.wait_for(link::mqtt::Status::Connected)) {
        return -1;
    }

    if (!m_config.is_set("offline")) {
        link::mqtt::configuration sink_mqtt_config {};
        sink_mqtt_config.host = m_config.get<std::string>("sink_mqtt_host");
        sink_mqtt_config.port = m_config.get<int>("sink_mqtt_port");
        sink_mqtt_config.login.username = m_config.get<std::string>("sink_mqtt_user");
        sink_mqtt_config.login.password = m_config.get<std::string>("sink_mqtt_password");
        sink_mqtt_link = std::make_unique<link::mqtt>(sink_mqtt_config, m_config.get<std::string>("station_id") + "_sink", "muon::mqtt:si");
        if (!sink_mqtt_link->wait_for(link::mqtt::Status::Connected)) {
            return -1;
        }
    }

    sink::collection<event_t> collection_event_sink { "muon::sink::e" };
    sink::collection<cluster_log_t> collection_clusterlog_sink { "muon::sink::c" };
    sink::collection<detector_summary_t> collection_detectorsummary_sink { "muon::sink::d" };
    sink::collection<trigger::detector> collection_trigger_sink { "muon::sink::t" };
    sink::collection<detector_log_t> collection_detectorlog_sink { "muon::sink::l" };

    if (m_config.is_set("debug")) {
        ascii_event_sink = std::make_unique<sink::ascii<event_t>>(std::cout);
        ascii_clusterlog_sink = std::make_unique<sink::ascii<cluster_log_t>>(std::cout);
        ascii_detectorsummary_sink = std::make_unique<sink::ascii<detector_summary_t>>(std::cout);
        ascii_trigger_sink = std::make_unique<sink::ascii<trigger::detector>>(std::cout);

        collection_event_sink.emplace(*ascii_event_sink);
        collection_clusterlog_sink.emplace(*ascii_clusterlog_sink);
        collection_detectorsummary_sink.emplace(*ascii_detectorsummary_sink);
        collection_trigger_sink.emplace(*ascii_trigger_sink);
    }

    if (!m_config.is_set("offline")) {
        mqtt_trigger_sink = std::make_unique<sink::mqtt<trigger::detector>>(sink_mqtt_link->publish("muonpi/trigger"));
        collection_trigger_sink.emplace(*mqtt_trigger_sink);

        if (!m_config.is_set("local")) {
            link::influx::configuration influx_config {};

            influx_config.host = m_config.get<std::string>("influx_host");
            influx_config.database = m_config.get<std::string>("influx_database");
            influx_config.login.username = m_config.get<std::string>("influx_user");
            influx_config.login.password = m_config.get<std::string>("influx_password");

            db_link = std::make_unique<link::influx>(influx_config);

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

    m_supervisor = std::make_unique<supervision::state>(
        collection_clusterlog_sink,
        supervision::state::configuration {
            m_config.get<std::string>("station_id"),
            std::chrono::minutes { m_config.get<int>("clusterlog_interval") } });
    coincidence_filter coincidencefilter { collection_event_sink, *m_supervisor };
    supervision::timebase timebasesupervisor { coincidencefilter, coincidencefilter };
    supervision::station stationsupervisor {
        collection_detectorsummary_sink,
        collection_trigger_sink,
        timebasesupervisor,
        timebasesupervisor,
        *m_supervisor,
        supervision::station::configuration {
            m_config.get<std::string>("station_id"),
            std::chrono::minutes { m_config.get<int>("detectorsummary_interval") } }
    };

    source::mqtt<event_t> event_source {
        stationsupervisor,
        source_mqtt_link.subscribe("muonpi/data/#"),
        source::mqtt<event_t>::configuration { m_config.get<int>("geohash_length") }
    };
    source::mqtt<event_t> l1_source {
        stationsupervisor,
        source_mqtt_link.subscribe("muonpi/l1data/#"),
        source::mqtt<event_t>::configuration { m_config.get<int>("geohash_length") }
    };
    source::mqtt<detector_info_t<location_t>> detector_location_source {
        stationsupervisor,
        source_mqtt_link.subscribe("muonpi/log/#"),
        source::mqtt<detector_info_t<location_t>>::configuration {
            m_config.get<int>("geohash_length") }
    };

    source::mqtt<detector_log_t> detectorlog_source {
        collection_detectorlog_sink,
        source_mqtt_link.subscribe("muonpi/log/#"),
        source::mqtt<detector_log_t>::configuration {
            m_config.get<int>("geohash_length") }
    };

    if (m_config.is_set("store_histogram") && m_config.get<bool>("store_histogram")) {
        stationcoincidence = std::make_unique<station_coincidence>(
            m_config.get<std::string>("histogram"),
            stationsupervisor,
            station_coincidence::configuration {
                std::chrono::hours { m_config.get<int>("histogram_sample_time") } });

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
