#include "defaults.h"

#include <muonpi/log.h>

#include <fstream>

#include <filesystem>

namespace muonpi::Config {

auto setup(int argc, const char* argv[]) -> std::optional<config>
{

    namespace po = boost::program_options;

    config cfg {};

    auto desc = cfg.setup("General options");

    desc.add_option("help,h", "produce help message");
    desc.add_option("offline,o", "Do not send processed data to the servers.");
    desc.add_option("debug,d", "Use the ascii sinks for debugging.");
    desc.add_option("local,l", "Run the cluser as a local instance");
    desc.add_option("verbose,v", po::value<int>()->default_value(Config::Default::meta.verbosity), "Verbosity level");
    desc.add_option("config,c", po::value<std::string>()->default_value(Config::Default::files.config), "Specify a configuration file to use");

    desc.commit(argc, argv);

    auto file = cfg.setup("Config file options");
    file.add_option("station_id", po::value<std::string>(), "Base station ID");
    file.add_option("sink_mqtt_base_path", po::value<std::string>()->default_value("muonpi/"), "Base path for the mqtt sink topics.");
    file.add_option("source_mqtt_base_path", po::value<std::string>()->default_value("muonpi/"), "Base path for the mqtt source topics.");

    file.add_option("source_mqtt_user", po::value<std::string>(), "MQTT User to use for the source");
    file.add_option("source_mqtt_password", po::value<std::string>(), "MQTT password to use for the source");
    file.add_option("source_mqtt_host", po::value<std::string>(), "MQTT hostname for the source");
    file.add_option("source_mqtt_port", po::value<int>(), "MQTT port for the source");

    file.add_option("sink_mqtt_user", po::value<std::string>(), "MQTT User to use for the sink");
    file.add_option("sink_mqtt_password", po::value<std::string>(), "MQTT password to use for the sink");
    file.add_option("sink_mqtt_host", po::value<std::string>(), "MQTT hostname for the sink");
    file.add_option("sink_mqtt_port", po::value<int>(), "MQTT port for the sink");

    file.add_option("influx_user", po::value<std::string>(), "InfluxDb Username");
    file.add_option("influx_password", po::value<std::string>(), "InfluxDb Password");
    file.add_option("influx_database", po::value<std::string>(), "InfluxDb Database");
    file.add_option("influx_host", po::value<std::string>(), "InfluxDB Hostname");

    file.add_option("ldap_bind_dn", po::value<std::string>(), "LDAP Bind DN");
    file.add_option("ldap_password", po::value<std::string>(), "LDAP Bind Password");
    file.add_option("ldap_host", po::value<std::string>(), "LDAP Hostname");

    file.add_option("store_histogram", po::value<bool>()->default_value(false), "Track and store histograms.");
    file.add_option("histogram", po::value<std::string>()->default_value("data"), "Storage location of the histograms");
    file.add_option("histogram_sample_time", po::value<int>()->default_value(std::chrono::duration_cast<std::chrono::hours>(Config::Default::interval.histogram_sample_time).count()), "histogram sample time to use. In hours.");
    file.add_option("geohash_length", po::value<int>()->default_value(Config::Default::meta.max_geohash_length), "Geohash length to use");
    file.add_option("clusterlog_interval", po::value<int>()->default_value(std::chrono::duration_cast<std::chrono::minutes>(Config::Default::interval.clusterlog).count()), "Interval in which to send the cluster log. In minutes.");
    file.add_option("detectorsummary_interval", po::value<int>()->default_value(std::chrono::duration_cast<std::chrono::minutes>(Config::Default::interval.detectorsummary).count()), "Interval in which to send the detector summary. In minutes.");

    if (cfg.is_set("help")) {
        log::info() << "\n"
                    << desc << '\n';
        return {};
    }

    std::string config_file { cfg.get<std::string>("config") };

    file.commit(config_file);

    return cfg;
}

} // namespace muonpi::Config

namespace muonpi::Version::dnp {
auto string() -> std::string
{
    return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch) + ((std::strlen(additional) > 0)?("-" + std::string{additional}):("")) + ((std::strlen(hash) > 0)?("-" + std::string{hash}):(""));
}
} // namespace muonpi::Version::dnp
