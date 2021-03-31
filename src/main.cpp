#include "coincidencefilter.h"
#include "defaults.h"
#include "detectortracker.h"
#include "triggerhandler.h"
#include "utility/configuration.h"
#include "utility/log.h"
#include "utility/parameters.h"

#include "link/mqtt.h"
#include "sink/mqtt.h"
#include "source/mqtt.h"
#include "supervision/state.h"
#include "supervision/timebase.h"

#include "link/database.h"
#include "sink/database.h"

#include "sink/ascii.h"

#include "messages/clusterlog.h"
#include "messages/detectorsummary.h"

#include <csignal>
#include <functional>

void signal_handler(int signal);

static std::function<void(int)> shutdown_handler;

void signal_handler(int signal)
{
    shutdown_handler(signal);
}

template <typename T>
struct sinkGuard {
    muonpi::sink::base<T>* sink { nullptr };
    ~sinkGuard()
    {
        delete sink;
    }
};
auto main(int argc, char* argv[]) -> int
{

    muonpi::log::manager::singleton()->add_sink(std::make_shared<muonpi::log::syslog_sink>());

    muonpi::parameters parameters { "muondetector-custer", "Calculate coincidences for the muonpi network" };

    parameters
        << muonpi::parameters::definition { "c", "config", "Specify a configuration file to use", true }
        << muonpi::parameters::definition { "l", "credentials", "Specify a credentials file to use", true }
        << muonpi::parameters::definition { "s", "setup", "Setup the Credentials file from a plaintext file given with this option. The file will be written to the location given in the -l parameter in an encrypted format.", true }
        << muonpi::parameters::definition { "d", "debug", "Additionally to the normal sinks use ascii sinks for debugging. Also enables the log output to stderr." };

    if (!parameters.start(argc, argv)) {
        return 0;
    }
    if (parameters["d"]) {
        muonpi::log::manager::singleton()->add_sink(std::make_shared<muonpi::log::stream_sink>(std::cerr));
    }
    if (parameters["l"]) {
        muonpi::Config::files.credentials = parameters["l"].value;
    }
    if (parameters["c"]) {
        muonpi::Config::files.config = parameters["c"].value;
    }

    if (parameters["s"]) {
        if (!parameters["l"]) {
            std::cout << "No credentials location given, using default.\n";
        }

        muonpi::configuration credentials { parameters["s"].value };
        credentials
            << muonpi::configuration::definition { "source_mqtt_user", &muonpi::Config::source_mqtt.login.username }
            << muonpi::configuration::definition { "source_mqtt_password", &muonpi::Config::source_mqtt.login.password }
            << muonpi::configuration::definition { "source_mqtt_station_id", &muonpi::Config::source_mqtt.login.station_id }

            << muonpi::configuration::definition { "sink_mqtt_user", &muonpi::Config::sink_mqtt.login.username }
            << muonpi::configuration::definition { "sink_mqtt_password", &muonpi::Config::sink_mqtt.login.password }
            << muonpi::configuration::definition { "sink_mqtt_station_id", &muonpi::Config::sink_mqtt.login.station_id }

            << muonpi::configuration::definition { "influx_user", &muonpi::Config::influx.login.username }
            << muonpi::configuration::definition { "influx_password", &muonpi::Config::influx.login.password }
            << muonpi::configuration::definition { "influx_database", &muonpi::Config::influx.database }

            << muonpi::configuration::definition { "ldap_bind_dn", &muonpi::Config::ldap.login.bind_dn }
            << muonpi::configuration::definition { "ldap_password", &muonpi::Config::ldap.login.password };
        if (!credentials.read()) {
            std::cout << "Could not read input file.\n";
            return 1;
        }

        credentials.set_encrypted(true);
        credentials.set_filename(muonpi::Config::files.credentials);

        if (credentials.write()) {
            std::cout << "Wrote credentials file.\n";
            return 0;
        }
        std::cout << "Could not write credentials file.\n";
        return 1;
    }

    muonpi::configuration config { muonpi::Config::files.config };
    config
        << muonpi::configuration::definition { "source_mqtt_host", &muonpi::Config::source_mqtt.host }
        << muonpi::configuration::definition { "source_mqtt_port", &muonpi::Config::source_mqtt.port }

        << muonpi::configuration::definition { "sink_mqtt_host", &muonpi::Config::sink_mqtt.host }
        << muonpi::configuration::definition { "sink_mqtt_port", &muonpi::Config::sink_mqtt.port }

        << muonpi::configuration::definition { "influx_host", &muonpi::Config::influx.host }
        << muonpi::configuration::definition { "influx_cluster_id", &muonpi::Config::influx.cluster_id }

        << muonpi::configuration::definition { "ldap_host", &muonpi::Config::ldap.server }

        << muonpi::configuration::definition { "rest_port", &muonpi::Config::rest.port }
        << muonpi::configuration::definition { "rest_trigger_file", &muonpi::Config::trigger.save_file }
        << muonpi::configuration::definition { "rest_cert", &muonpi::Config::rest.cert }
        << muonpi::configuration::definition { "rest_privkey", &muonpi::Config::rest.privkey }
        << muonpi::configuration::definition { "rest_fullchain", &muonpi::Config::rest.fullchain }

        << muonpi::configuration::definition { "run_local_cluster", &muonpi::Config::meta.local_cluster }
        << muonpi::configuration::definition { "max_geohash_length", &muonpi::Config::meta.max_geohash_length };
    if (!parameters["l"]) {
        config << muonpi::configuration::definition { "credentials_file", &muonpi::Config::files.credentials };
    }
    if (!config.read()) {
        std::cout << "Could not read configuration file.\n";
        return 1;
    }

    muonpi::configuration credentials { muonpi::Config::files.credentials, true };

    credentials
        << muonpi::configuration::definition { "source_mqtt_user", &muonpi::Config::source_mqtt.login.username }
        << muonpi::configuration::definition { "source_mqtt_password", &muonpi::Config::source_mqtt.login.password }
        << muonpi::configuration::definition { "source_mqtt_station_id", &muonpi::Config::source_mqtt.login.station_id }

        << muonpi::configuration::definition { "sink_mqtt_user", &muonpi::Config::sink_mqtt.login.username }
        << muonpi::configuration::definition { "sink_mqtt_password", &muonpi::Config::sink_mqtt.login.password }
        << muonpi::configuration::definition { "sink_mqtt_station_id", &muonpi::Config::sink_mqtt.login.station_id }

        << muonpi::configuration::definition { "influx_user", &muonpi::Config::influx.login.username }
        << muonpi::configuration::definition { "influx_password", &muonpi::Config::influx.login.password }
        << muonpi::configuration::definition { "influx_database", &muonpi::Config::influx.database }

        << muonpi::configuration::definition { "ldap_bind_dn", &muonpi::Config::ldap.login.bind_dn }
        << muonpi::configuration::definition { "ldap_password", &muonpi::Config::ldap.login.password };
    if (!credentials.read()) {
        std::cout << "Could not read credentials file.\n";
        return 1;
    }

    muonpi::link::mqtt source_mqtt_link { muonpi::Config::source_mqtt };

    if (!source_mqtt_link.wait_for(muonpi::link::mqtt::Status::Connected)) {
        return -1;
    }

    muonpi::link::mqtt sink_mqtt_link { muonpi::Config::sink_mqtt };
    if (!sink_mqtt_link.wait_for(muonpi::link::mqtt::Status::Connected)) {
        return -1;
    }

    muonpi::sink::mqtt<muonpi::trigger::detector> trigger_sink { sink_mqtt_link.publish("muonpi/trigger") };

    struct Guard {
        sinkGuard<muonpi::event_t> event {};
        sinkGuard<muonpi::cluster_log_t> clusterlog {};
        sinkGuard<muonpi::detetor_summary_t> detectorsummary {};
        muonpi::link::database* db_link { nullptr };
        ~Guard()
        {
            delete db_link;
        }
    };
    std::array<Guard, 4> guard {};

    sinkGuard<muonpi::detector_log_t> detectorlog {};

    if (!muonpi::Config::meta.local_cluster) {
        guard[0].db_link = new muonpi::link::database { muonpi::Config::influx };

        guard[0].event.sink = new muonpi::sink::database<muonpi::event_t> { *guard[0].db_link };
        guard[0].clusterlog.sink = new muonpi::sink::database<muonpi::cluster_log_t> { *guard[0].db_link };
        guard[0].detectorsummary.sink = new muonpi::sink::database<muonpi::detetor_summary_t> { *guard[0].db_link };

        guard[3].event.sink = new muonpi::sink::mqtt<muonpi::event_t> { sink_mqtt_link.publish("muonpi/events") };

        detectorlog.sink = new muonpi::sink::database<muonpi::detector_log_t> { *guard[0].db_link };
    } else {
        guard[0].event.sink = new muonpi::sink::mqtt<muonpi::event_t> { sink_mqtt_link.publish("muonpi/l1data") };
        guard[0].clusterlog.sink = new muonpi::sink::mqtt<muonpi::cluster_log_t> { sink_mqtt_link.publish("muonpi/cluster") };
        guard[0].detectorsummary.sink = new muonpi::sink::mqtt<muonpi::detetor_summary_t> { sink_mqtt_link.publish("muonpi/cluster") };
        detectorlog.sink = new muonpi::sink::mqtt<muonpi::detector_log_t> { sink_mqtt_link.publish("muonpi/log/") };

        reinterpret_cast<muonpi::sink::mqtt<muonpi::event_t>*>(guard[0].event.sink)->set_detailed();
    }

    if (parameters["d"]) {
        guard[1].event.sink = new muonpi::sink::ascii<muonpi::event_t> { std::cout };
        guard[1].clusterlog.sink = new muonpi::sink::ascii<muonpi::cluster_log_t> { std::cout };
        guard[1].detectorsummary.sink = new muonpi::sink::ascii<muonpi::detetor_summary_t> { std::cout };

        if (!muonpi::Config::meta.local_cluster) {
            guard[2].event.sink = new muonpi::sink::collection<muonpi::event_t, 3> { { guard[1].event.sink, guard[0].event.sink, guard[3].event.sink } };
        } else {
            guard[2].event.sink = new muonpi::sink::collection<muonpi::event_t, 2> { { guard[1].event.sink, guard[0].event.sink } };
        }
        guard[2].clusterlog.sink = new muonpi::sink::collection<muonpi::cluster_log_t, 2> { { guard[1].clusterlog.sink, guard[0].clusterlog.sink } };
        guard[2].detectorsummary.sink = new muonpi::sink::collection<muonpi::detetor_summary_t, 2> { { guard[1].detectorsummary.sink, guard[0].detectorsummary.sink } };
    } else {
        if (!muonpi::Config::meta.local_cluster) {
            guard[2].event.sink = new muonpi::sink::collection<muonpi::event_t, 2> { { guard[0].event.sink, guard[3].event.sink } };
        } else {
            guard[2].event.sink = new muonpi::sink::collection<muonpi::event_t, 1> { { guard[0].event.sink } };
        }
        guard[2].clusterlog.sink = new muonpi::sink::collection<muonpi::cluster_log_t, 1> { { guard[0].clusterlog.sink } };
        guard[2].detectorsummary.sink = new muonpi::sink::collection<muonpi::detetor_summary_t, 1> { { guard[0].detectorsummary.sink } };
    }

    muonpi::state_supervisor supervisor { *guard[2].clusterlog.sink };
    muonpi::coincidence_filter coincidencefilter { *guard[2].event.sink, supervisor };
    muonpi::timebase_supervisor timebasesupervisor { coincidencefilter, coincidencefilter };
    muonpi::detector_tracker detectortracker { *guard[2].detectorsummary.sink, trigger_sink, timebasesupervisor, timebasesupervisor, supervisor };
    muonpi::trigger_handler triggerhandler { detectortracker, muonpi::Config::ldap, muonpi::Config::trigger };
    muonpi::rest::service rest_service { muonpi::Config::rest };

    muonpi::source::mqtt<muonpi::event_t> event_source { detectortracker, source_mqtt_link.subscribe("muonpi/data/#") };
    muonpi::source::mqtt<muonpi::event_t> l1_source { detectortracker, source_mqtt_link.subscribe("muonpi/l1data/#") };
    muonpi::source::mqtt<muonpi::detetor_info_t<muonpi::location_t>> detector_location_source { detectortracker, source_mqtt_link.subscribe("muonpi/log/#") };

    muonpi::source::mqtt<muonpi::detector_log_t> detectorlog_source { *detectorlog.sink, source_mqtt_link.subscribe("muonpi/log/#") };

    rest_service.add_handler(&triggerhandler);

    supervisor.add_thread(&detectortracker);
    supervisor.add_thread(&sink_mqtt_link);
    supervisor.add_thread(&source_mqtt_link);
    supervisor.add_thread(&rest_service);
    supervisor.add_thread(dynamic_cast<muonpi::sink::threaded<muonpi::event_t>*>(guard[2].event.sink));
    supervisor.add_thread(dynamic_cast<muonpi::sink::threaded<muonpi::detetor_summary_t>*>(guard[2].detectorsummary.sink));
    supervisor.add_thread(dynamic_cast<muonpi::sink::threaded<muonpi::cluster_log_t>*>(guard[2].clusterlog.sink));

    shutdown_handler = [&](int signal) {
        if (
            (signal == SIGINT)
            || (signal == SIGTERM)
            || (signal == SIGHUP)) {
            muonpi::log::notice() << "Received signal: " + std::to_string(signal) + ". Exiting.";
            supervisor.stop();
            coincidencefilter.stop();
        }
    };

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGHUP, signal_handler);

    coincidencefilter.start_synchronuos();

    return coincidencefilter.wait();
}
