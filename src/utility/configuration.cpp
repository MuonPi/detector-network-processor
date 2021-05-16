#include "utility/configuration.h"

#include "utility/log.h"

#include <fstream>

namespace muonpi {


const std::unique_ptr<config> config::s_singleton { std::make_unique<config>() };

auto config::singleton() -> const std::unique_ptr<config>&
{
    return s_singleton;
}


auto config::setup(int argc, const char* argv[]) -> bool
{
    namespace po = boost::program_options;

    po::options_description desc("General options");
    desc.add_options()
            ("help,h", "produce help message")
            ("offline,o", "Do not send processed data to the servers.")
            ("debug,d", "Use the ascii sinks for debugging.")
            ("local,l", "Run the cluser as a local instance")
            ("verbose,v", po::value<int>()->default_value(meta.verbosity), "Verbosity level")
            ("config,c", po::value<std::string>()->default_value(files.config), "Specify a configuration file to use");

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
            ("histogram_sample_time", po::value<int>()->default_value(std::chrono::duration_cast<std::chrono::hours>(interval.histogram_sample_time).count()), "histogram sample time to use. In hours.")
            ("geohash_length", po::value<int>()->default_value(meta.max_geohash_length), "Geohash length to use")
            ("clusterlog_interval", po::value<int>()->default_value(std::chrono::duration_cast<std::chrono::minutes>(interval.clusterlog).count()), "Interval in which to send the cluster log. In minutes.")
            ("detectorsummary_interval", po::value<int>()->default_value(std::chrono::duration_cast<std::chrono::minutes>(interval.detectorsummary).count()), "Interval in which to send the detector summary. In minutes.")
            ;

    po::store(po::parse_command_line(argc, argv, desc), m_options);


    if (option_set("help")) {
        log::info() << "\n" << desc << '\n';
        return false;
    }
    if (option_set("config")) {
        std::ifstream ifs { get_option<std::string>("config") };
        if (ifs) {
            po::store(po::parse_config_file(ifs, file_options), m_options);
        } else {
            std::cerr << "Could not open configuration file.\n";
        }
    }
    po::notify(m_options);

    if (option_set("histogram_sample_time")) {
        interval.histogram_sample_time = std::chrono::hours { get_option<int>("histogram_sample_time") };
    }

    if (option_set("clusterlog_interval")) {
        interval.clusterlog = std::chrono::minutes { get_option<int>("clusterlog_interval") };
    }

    if (option_set("detectorsummary_interval")) {
        interval.detectorsummary = std::chrono::minutes { get_option<int>("detectorsummary_interval") };
    }


    check_option("verbose", meta.verbosity);

    check_option("config", files.config);

    check_option("station_id", meta.station);

    check_option("source_mqtt_user", source_mqtt.login.username);
    check_option("source_mqtt_password", source_mqtt.login.password);
    check_option("source_mqtt_host", source_mqtt.host);
    check_option("source_mqtt_port", source_mqtt.port);

    check_option("sink_mqtt_user", sink_mqtt.login.username);
    check_option("sink_mqtt_password", sink_mqtt.login.password);
    check_option("sink_mqtt_host", sink_mqtt.host);
    check_option("sink_mqtt_port", sink_mqtt.port);

    check_option("influx_user", influx.login.username);
    check_option("influx_password", influx.login.password);
    check_option("influx_database", influx.database);
    check_option("influx_host", influx.host);

    check_option("ldap_bind_dn", ldap.login.bind_dn);
    check_option("ldap_password", ldap.login.password);
    check_option("ldap_host", ldap.host);

    check_option("geohash_length", meta.max_geohash_length);

    return true;
}

} // namespace muonpi
