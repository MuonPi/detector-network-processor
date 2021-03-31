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

    muonpi::Log::Log::singleton()->add_sink(std::make_shared<muonpi::Log::syslog_sink>());

    muonpi::Parameters parameters { "muondetector-custer", "Calculate coincidences for the muonpi network" };

    parameters
        << muonpi::Parameters::Definition { "c", "config", "Specify a configuration file to use", true }
        << muonpi::Parameters::Definition { "l", "credentials", "Specify a credentials file to use", true }
        << muonpi::Parameters::Definition { "s", "setup", "Setup the Credentials file from a plaintext file given with this option. The file will be written to the location given in the -l parameter in an encrypted format.", true }
        << muonpi::Parameters::Definition { "d", "debug", "Additionally to the normal sinks use ascii sinks for debugging. Also enables the log output to stderr." };

    if (!parameters.start(argc, argv)) {
        return 0;
    }
    if (parameters["d"]) {
        muonpi::Log::Log::singleton()->add_sink(std::make_shared<muonpi::Log::stream_sink>(std::cerr));
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

        muonpi::Configuration credentials { parameters["s"].value };
        credentials
            << muonpi::Option { "source_mqtt_user", &muonpi::Config::source_mqtt.login.username }
            << muonpi::Option { "source_mqtt_password", &muonpi::Config::source_mqtt.login.password }
            << muonpi::Option { "source_mqtt_station_id", &muonpi::Config::source_mqtt.login.station_id }

            << muonpi::Option { "sink_mqtt_user", &muonpi::Config::sink_mqtt.login.username }
            << muonpi::Option { "sink_mqtt_password", &muonpi::Config::sink_mqtt.login.password }
            << muonpi::Option { "sink_mqtt_station_id", &muonpi::Config::sink_mqtt.login.station_id }

            << muonpi::Option { "influx_user", &muonpi::Config::influx.login.username }
            << muonpi::Option { "influx_password", &muonpi::Config::influx.login.password }
            << muonpi::Option { "influx_database", &muonpi::Config::influx.database }

            << muonpi::Option { "ldap_bind_dn", &muonpi::Config::ldap.login.bind_dn }
            << muonpi::Option { "ldap_password", &muonpi::Config::ldap.login.password };
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

    muonpi::Configuration config { muonpi::Config::files.config };
    config
        << muonpi::Option { "source_mqtt_host", &muonpi::Config::source_mqtt.host }
        << muonpi::Option { "source_mqtt_port", &muonpi::Config::source_mqtt.port }

        << muonpi::Option { "sink_mqtt_host", &muonpi::Config::sink_mqtt.host }
        << muonpi::Option { "sink_mqtt_port", &muonpi::Config::sink_mqtt.port }

        << muonpi::Option { "influx_host", &muonpi::Config::influx.host }
        << muonpi::Option { "influx_cluster_id", &muonpi::Config::influx.cluster_id }

        << muonpi::Option { "ldap_host", &muonpi::Config::ldap.server }

        << muonpi::Option { "rest_port", &muonpi::Config::rest.port }
        << muonpi::Option { "rest_trigger_file", &muonpi::Config::trigger.save_file }
        << muonpi::Option { "rest_cert", &muonpi::Config::rest.cert }
        << muonpi::Option { "rest_privkey", &muonpi::Config::rest.privkey }
        << muonpi::Option { "rest_fullchain", &muonpi::Config::rest.fullchain }

        << muonpi::Option { "run_local_cluster", &muonpi::Config::meta.local_cluster }
        << muonpi::Option { "max_geohash_length", &muonpi::Config::meta.max_geohash_length };
    if (!parameters["l"]) {
        config << muonpi::Option { "credentials_file", &muonpi::Config::files.credentials };
    }
    if (!config.read()) {
        std::cout << "Could not read configuration file.\n";
        return 1;
    }

    muonpi::Configuration credentials { muonpi::Config::files.credentials, true };

    credentials
        << muonpi::Option { "source_mqtt_user", &muonpi::Config::source_mqtt.login.username }
        << muonpi::Option { "source_mqtt_password", &muonpi::Config::source_mqtt.login.password }
        << muonpi::Option { "source_mqtt_station_id", &muonpi::Config::source_mqtt.login.station_id }

        << muonpi::Option { "sink_mqtt_user", &muonpi::Config::sink_mqtt.login.username }
        << muonpi::Option { "sink_mqtt_password", &muonpi::Config::sink_mqtt.login.password }
        << muonpi::Option { "sink_mqtt_station_id", &muonpi::Config::sink_mqtt.login.station_id }

        << muonpi::Option { "influx_user", &muonpi::Config::influx.login.username }
        << muonpi::Option { "influx_password", &muonpi::Config::influx.login.password }
        << muonpi::Option { "influx_database", &muonpi::Config::influx.database }

        << muonpi::Option { "ldap_bind_dn", &muonpi::Config::ldap.login.bind_dn }
        << muonpi::Option { "ldap_password", &muonpi::Config::ldap.login.password };
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

    muonpi::sink::mqtt<muonpi::Trigger::Detector> trigger_sink { sink_mqtt_link.publish("muonpi/trigger") };

    struct Guard {
        sinkGuard<muonpi::Event> event {};
        sinkGuard<muonpi::ClusterLog> clusterlog {};
        sinkGuard<muonpi::DetectorSummary> detectorsummary {};
        muonpi::link::database* db_link { nullptr };
        ~Guard()
        {
            delete db_link;
        }
    };
    std::array<Guard, 4> guard {};

    sinkGuard<muonpi::DetectorLog> detectorlog {};

    if (!muonpi::Config::meta.local_cluster) {
        guard[0].db_link = new muonpi::link::database { muonpi::Config::influx };

        guard[0].event.sink = new muonpi::sink::database<muonpi::Event> { *guard[0].db_link };
        guard[0].clusterlog.sink = new muonpi::sink::database<muonpi::ClusterLog> { *guard[0].db_link };
        guard[0].detectorsummary.sink = new muonpi::sink::database<muonpi::DetectorSummary> { *guard[0].db_link };

        guard[3].event.sink = new muonpi::sink::mqtt<muonpi::Event> { sink_mqtt_link.publish("muonpi/events") };

        detectorlog.sink = new muonpi::sink::database<muonpi::DetectorLog> { *guard[0].db_link };
    } else {
        guard[0].event.sink = new muonpi::sink::mqtt<muonpi::Event> { sink_mqtt_link.publish("muonpi/l1data") };
        guard[0].clusterlog.sink = new muonpi::sink::mqtt<muonpi::ClusterLog> { sink_mqtt_link.publish("muonpi/cluster") };
        guard[0].detectorsummary.sink = new muonpi::sink::mqtt<muonpi::DetectorSummary> { sink_mqtt_link.publish("muonpi/cluster") };
        detectorlog.sink = new muonpi::sink::mqtt<muonpi::DetectorLog> { sink_mqtt_link.publish("muonpi/log/") };

        reinterpret_cast<muonpi::sink::mqtt<muonpi::Event>*>(guard[0].event.sink)->set_detailed();
    }

    if (parameters["d"]) {
        guard[1].event.sink = new muonpi::sink::ascii<muonpi::Event> { std::cout };
        guard[1].clusterlog.sink = new muonpi::sink::ascii<muonpi::ClusterLog> { std::cout };
        guard[1].detectorsummary.sink = new muonpi::sink::ascii<muonpi::DetectorSummary> { std::cout };

        if (!muonpi::Config::meta.local_cluster) {
            guard[2].event.sink = new muonpi::sink::collection<muonpi::Event, 3> { { guard[1].event.sink, guard[0].event.sink, guard[3].event.sink } };
        } else {
            guard[2].event.sink = new muonpi::sink::collection<muonpi::Event, 2> { { guard[1].event.sink, guard[0].event.sink } };
        }
        guard[2].clusterlog.sink = new muonpi::sink::collection<muonpi::ClusterLog, 2> { { guard[1].clusterlog.sink, guard[0].clusterlog.sink } };
        guard[2].detectorsummary.sink = new muonpi::sink::collection<muonpi::DetectorSummary, 2> { { guard[1].detectorsummary.sink, guard[0].detectorsummary.sink } };
    } else {
        if (!muonpi::Config::meta.local_cluster) {
            guard[2].event.sink = new muonpi::sink::collection<muonpi::Event, 2> { { guard[0].event.sink, guard[3].event.sink } };
        } else {
            guard[2].event.sink = new muonpi::sink::collection<muonpi::Event, 1> { { guard[0].event.sink } };
        }
        guard[2].clusterlog.sink = new muonpi::sink::collection<muonpi::ClusterLog, 1> { { guard[0].clusterlog.sink } };
        guard[2].detectorsummary.sink = new muonpi::sink::collection<muonpi::DetectorSummary, 1> { { guard[0].detectorsummary.sink } };
    }

    muonpi::state_supervisor supervisor { *guard[2].clusterlog.sink };
    muonpi::coincidence_filter coincidencefilter { *guard[2].event.sink, supervisor };
    muonpi::timebase_supervisor timebasesupervisor { coincidencefilter, coincidencefilter };
    muonpi::detector_tracker detectortracker { *guard[2].detectorsummary.sink, trigger_sink, timebasesupervisor, timebasesupervisor, supervisor };
    muonpi::trigger_handler triggerhandler { detectortracker, muonpi::Config::ldap, muonpi::Config::trigger };
    muonpi::rest::service rest_service { muonpi::Config::rest };

    muonpi::source::mqtt<muonpi::Event> event_source { detectortracker, source_mqtt_link.subscribe("muonpi/data/#") };
    muonpi::source::mqtt<muonpi::Event> l1_source { detectortracker, source_mqtt_link.subscribe("muonpi/l1data/#") };
    muonpi::source::mqtt<muonpi::DetectorInfo<muonpi::Location>> detector_location_source { detectortracker, source_mqtt_link.subscribe("muonpi/log/#") };

    muonpi::source::mqtt<muonpi::DetectorLog> detectorlog_source { *detectorlog.sink, source_mqtt_link.subscribe("muonpi/log/#") };

    rest_service.add_handler(&triggerhandler);

    supervisor.add_thread(&detectortracker);
    supervisor.add_thread(&sink_mqtt_link);
    supervisor.add_thread(&source_mqtt_link);
    supervisor.add_thread(&rest_service);
    supervisor.add_thread(dynamic_cast<muonpi::sink::threaded<muonpi::Event>*>(guard[2].event.sink));
    supervisor.add_thread(dynamic_cast<muonpi::sink::threaded<muonpi::DetectorSummary>*>(guard[2].detectorsummary.sink));
    supervisor.add_thread(dynamic_cast<muonpi::sink::threaded<muonpi::ClusterLog>*>(guard[2].clusterlog.sink));

    shutdown_handler = [&](int signal) {
        if (
            (signal == SIGINT)
            || (signal == SIGTERM)
            || (signal == SIGHUP)) {
            muonpi::Log::notice() << "Received signal: " + std::to_string(signal) + ". Exiting.";
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
