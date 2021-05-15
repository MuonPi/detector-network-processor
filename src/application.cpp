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

#include "utility/exceptions.h"

#include <exception>
#include <memory>


namespace muonpi {

std::function<void(int)> application::s_shutdown_handler;

void wrapper_signal_handler(int signal)
{
    application::s_shutdown_handler(signal);
}

auto application::setup(int argc, const char* argv[]) -> bool
{
    namespace po = boost::program_options;

    po::options_description desc("General options");
    desc.add_options()
            ("help,h", "produce help message")
            ("offline,o", "Do not send processed data to the servers.")
            ("debug,d", "Use the ascii sinks for debugging.")
            ("local,l", "Run the cluser as a local instance")
            ("config,c", po::value<std::string>()->default_value(Config::Default::files.config), "Specify a configuration file to use")
    ;

    po::options_description file_options("Config file options");
    file_options.add_options()
            ("station_id", po::value<std::string>(), "Base station ID")

            ("source_mqtt_user", po::value<std::string>(), "MQTT User to use for the source")
            ("source_mqtt_password", po::value<std::string>(), "MQTT password to use for the source")
            ("source_mqtt_host", po::value<std::string>(), "MQTT hostname for the source")
            ("source_mqtt_port", po::value<int>(), "MQTT port for the source")

            ("sink_mqtt_user", po::value<std::string>(), "MQTT User to use for the sink")
            ("sink_mqtt_password", po::value<std::string>(), "MQTT password to use for the sink")
            ("sink_mqtt_host", po::value<std::string>(), "MQTT hostname for the sink")
            ("sink_mqtt_port", po::value<int>(), "MQTT port for the sink")

            ("influx_user", po::value<std::string>(), "InfluxDb Username")
            ("influx_password", po::value<std::string>(), "InfluxDb Password")
            ("influx_database", po::value<std::string>(), "InfluxDb Database")
            ("influx_host", po::value<std::string>(), "InfluxDB Hostname")

            ("ldap_bind_dn", po::value<std::string>(), "LDAP Bind DN")
            ("ldap_password", po::value<std::string>(), "LDAP Bind Password")
            ("ldap_host", po::value<std::string>(), "LDAP Hostname")

            ("histogram", po::value<std::string>()->default_value("data"), "Track and store histograms. The parameter is the save directory")
            ("histogram_sample_time", po::value<int>()->default_value(std::chrono::duration_cast<std::chrono::hours>(Config::Default::interval.histogram_sample_time).count()), "histogram sample time to use. In hours.")
            ("geohash_length", po::value<int>()->default_value(Config::Default::meta.max_geohash_length), "Geohash length to use")
    ;

    po::store(po::parse_command_line(argc, argv, desc), m_options);
    if (option_set("help")) {
        std::cout<<"muondetector-cluster " << Version::string()<<"\n\n"<<desc<<'\n';
        return false;
    }
    if (option_set("config"))
    {
        std::ifstream ifs { get_option<std::string>("config") };
        if (ifs) {
            po::store(po::parse_config_file(ifs, file_options), m_options);
        } else {
            std::cerr<<"Could not open configuration file.\n";
        }
    }
    po::notify(m_options);


    std::set_terminate(error::terminate_handler);

    if (option_set("debug")) {
        log::manager::singleton()->add_sink(std::make_shared<log::stream_sink>(std::cerr));
    } else {
        log::manager::singleton()->add_sink(std::make_shared<log::syslog_sink>());
    }

    if (option_set("offline")) {
        log::info()<<"Starting in offline mode.";
    }

    if (option_set("histogram_sample_time")) {
        Config::interval.histogram_sample_time = std::chrono::hours { get_option<int>("histogram_sample_time")};
    }

    log::info() << "muondetector-cluster " + Version::string();

    check_option("config", Config::files.config);

    check_option("station_id", Config::meta.station);

    check_option("source_mqtt_user", Config::source_mqtt.login.username);
    check_option("source_mqtt_password", Config::source_mqtt.login.password);
    check_option("source_mqtt_host", Config::source_mqtt.host);
    check_option("source_mqtt_port", Config::source_mqtt.port);

    check_option("sink_mqtt_user", Config::sink_mqtt.login.username);
    check_option("sink_mqtt_password", Config::sink_mqtt.login.password);
    check_option("sink_mqtt_host", Config::sink_mqtt.host);
    check_option("sink_mqtt_port", Config::sink_mqtt.port);

    check_option("influx_user", Config::influx.login.username);
    check_option("influx_password", Config::influx.login.password);
    check_option("influx_database", Config::influx.database);
    check_option("influx_host", Config::influx.host);

    check_option("ldap_bind_dn", Config::ldap.login.bind_dn);
    check_option("ldap_password", Config::ldap.login.password);
    check_option("ldap_host", Config::ldap.host);

    check_option("geohash_length", Config::meta.max_geohash_length);

    return true;
}

template <typename T>
using sink_ptr = std::unique_ptr<sink::base<T>>;

auto application::run() -> int
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

    link::mqtt source_mqtt_link { Config::source_mqtt, Config::meta.station + "_sink", "muon::mqtt::so" };
    if (!source_mqtt_link.wait_for(link::mqtt::Status::Connected)) {
        return -1;
    }

    if (!option_set("offline")) {
        sink_mqtt_link = std::make_unique<link::mqtt>(Config::sink_mqtt, Config::meta.station + "_sink", "muon::mqtt:si");
        if (!sink_mqtt_link->wait_for(link::mqtt::Status::Connected)) {
            return -1;
        }
    }

    sink::collection<event_t> collection_event_sink { "muon::sink::e" };
    sink::collection<cluster_log_t> collection_clusterlog_sink { "muon::sink::c" };
    sink::collection<detector_summary_t> collection_detectorsummary_sink { "muon::sink::d" };
    sink::collection<trigger::detector> collection_trigger_sink { "muon::sink::t" };
    sink::collection<detector_log_t> collection_detectorlog_sink { "muon::sink::l" };

    if (option_set("debug")) {
        ascii_event_sink = std::make_unique<sink::ascii<event_t>>(std::cout);
        ascii_clusterlog_sink = std::make_unique<sink::ascii<cluster_log_t>>(std::cout);
        ascii_detectorsummary_sink = std::make_unique<sink::ascii<detector_summary_t>>(std::cout);
        ascii_trigger_sink = std::make_unique<sink::ascii<trigger::detector>>(std::cout);

        collection_event_sink.emplace(*ascii_event_sink);
        collection_clusterlog_sink.emplace(*ascii_clusterlog_sink);
        collection_detectorsummary_sink.emplace(*ascii_detectorsummary_sink);
        collection_trigger_sink.emplace(*ascii_trigger_sink);
    }

    if (!option_set("offline")) {
        mqtt_trigger_sink = std::make_unique<sink::mqtt<trigger::detector>>(sink_mqtt_link->publish("muonpi/trigger"));
        collection_trigger_sink.emplace(*mqtt_trigger_sink);

        if (!Config::meta.local_cluster) {
            db_link = std::make_unique<link::database>(Config::influx);

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

    if (option_set("histogram")) {
        stationcoincidence = std::make_unique<station_coincidence>(get_option<std::string>("histogram"), stationsupervisor);

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

    s_shutdown_handler = [this](int signal) { signal_handler(signal); };

    std::signal(SIGINT, wrapper_signal_handler);
    std::signal(SIGTERM, wrapper_signal_handler);
    std::signal(SIGHUP, wrapper_signal_handler);

    m_supervisor->start_synchronuos();

    return m_supervisor->wait();
}

void application::signal_handler(int signal)
{
    if ((signal == SIGINT) || (signal == SIGTERM) || (signal == SIGHUP)) {
        log::notice() << "Received signal: " + std::to_string(signal) + ". Exiting.";
        m_supervisor->stop();
    }
}

}
