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
struct SinkGuard {
    MuonPi::Sink::Base<T>* sink { nullptr };
    ~SinkGuard()
    {
        delete sink;
    }
};
auto main(int argc, char* argv[]) -> int
{

    MuonPi::Log::Log::singleton()->add_sink(std::make_shared<MuonPi::Log::SyslogSink>());

    MuonPi::Parameters parameters { "muondetector-custer", "Calculate coincidences for the MuonPi network" };

    parameters
        << MuonPi::Parameters::Definition { "c", "config", "Specify a configuration file to use", true }
        << MuonPi::Parameters::Definition { "l", "credentials", "Specify a credentials file to use", true }
        << MuonPi::Parameters::Definition { "s", "setup", "Setup the Credentials file from a plaintext file given with this option. The file will be written to the location given in the -l parameter in an encrypted format.", true }
        << MuonPi::Parameters::Definition { "d", "debug", "Additionally to the normal sinks use Ascii sinks for debugging. Also enables the log output to stderr." };

    if (!parameters.start(argc, argv)) {
        return 0;
    }
    if (parameters["d"]) {
        MuonPi::Log::Log::singleton()->add_sink(std::make_shared<MuonPi::Log::StreamSink>(std::cerr));
    }
    if (parameters["l"]) {
        MuonPi::Config::files.credentials = parameters["l"].value;
    }
    if (parameters["c"]) {
        MuonPi::Config::files.config = parameters["c"].value;
    }

    if (parameters["s"]) {
        if (!parameters["l"]) {
            std::cout << "No credentials location given, using default.\n";
        }

        MuonPi::Configuration credentials { parameters["s"].value };
        credentials
            << MuonPi::Option { "source_mqtt_user", &MuonPi::Config::source_mqtt.login.username }
            << MuonPi::Option { "source_mqtt_password", &MuonPi::Config::source_mqtt.login.password }
            << MuonPi::Option { "source_mqtt_station_id", &MuonPi::Config::source_mqtt.login.station_id }

            << MuonPi::Option { "sink_mqtt_user", &MuonPi::Config::sink_mqtt.login.username }
            << MuonPi::Option { "sink_mqtt_password", &MuonPi::Config::sink_mqtt.login.password }
            << MuonPi::Option { "sink_mqtt_station_id", &MuonPi::Config::sink_mqtt.login.station_id }

            << MuonPi::Option { "influx_user", &MuonPi::Config::influx.login.username }
            << MuonPi::Option { "influx_password", &MuonPi::Config::influx.login.password }
            << MuonPi::Option { "influx_database", &MuonPi::Config::influx.database }

            << MuonPi::Option { "ldap_bind_dn", &MuonPi::Config::ldap.login.bind_dn }
            << MuonPi::Option { "ldap_password", &MuonPi::Config::ldap.login.password };
        if (!credentials.read()) {
            std::cout << "Could not read input file.\n";
            return 1;
        }

        credentials.set_encrypted(true);
        credentials.set_filename(MuonPi::Config::files.credentials);

        if (credentials.write()) {
            std::cout << "Wrote credentials file.\n";
            return 0;
        }
        std::cout << "Could not write credentials file.\n";
        return 1;
    }

    MuonPi::Configuration config { MuonPi::Config::files.config };
    config
        << MuonPi::Option { "source_mqtt_host", &MuonPi::Config::source_mqtt.host }
        << MuonPi::Option { "source_mqtt_port", &MuonPi::Config::source_mqtt.port }

        << MuonPi::Option { "sink_mqtt_host", &MuonPi::Config::sink_mqtt.host }
        << MuonPi::Option { "sink_mqtt_port", &MuonPi::Config::sink_mqtt.port }

        << MuonPi::Option { "influx_host", &MuonPi::Config::influx.host }
        << MuonPi::Option { "influx_cluster_id", &MuonPi::Config::influx.cluster_id }

        << MuonPi::Option { "ldap_host", &MuonPi::Config::ldap.server }

        << MuonPi::Option { "rest_port", &MuonPi::Config::rest.port }
        << MuonPi::Option { "rest_trigger_file", &MuonPi::Config::trigger.save_file }
        << MuonPi::Option { "rest_cert", &MuonPi::Config::rest.cert }
        << MuonPi::Option { "rest_privkey", &MuonPi::Config::rest.privkey }
        << MuonPi::Option { "rest_fullchain", &MuonPi::Config::rest.fullchain }

        << MuonPi::Option { "run_local_cluster", &MuonPi::Config::meta.local_cluster }
        << MuonPi::Option { "max_geohash_length", &MuonPi::Config::meta.max_geohash_length };
    if (!parameters["l"]) {
        config << MuonPi::Option { "credentials_file", &MuonPi::Config::files.credentials };
    }
    if (!config.read()) {
        std::cout << "Could not read configuration file.\n";
        return 1;
    }

    MuonPi::Configuration credentials { MuonPi::Config::files.credentials, true };

    credentials
        << MuonPi::Option { "source_mqtt_user", &MuonPi::Config::source_mqtt.login.username }
        << MuonPi::Option { "source_mqtt_password", &MuonPi::Config::source_mqtt.login.password }
        << MuonPi::Option { "source_mqtt_station_id", &MuonPi::Config::source_mqtt.login.station_id }

        << MuonPi::Option { "sink_mqtt_user", &MuonPi::Config::sink_mqtt.login.username }
        << MuonPi::Option { "sink_mqtt_password", &MuonPi::Config::sink_mqtt.login.password }
        << MuonPi::Option { "sink_mqtt_station_id", &MuonPi::Config::sink_mqtt.login.station_id }

        << MuonPi::Option { "influx_user", &MuonPi::Config::influx.login.username }
        << MuonPi::Option { "influx_password", &MuonPi::Config::influx.login.password }
        << MuonPi::Option { "influx_database", &MuonPi::Config::influx.database }

        << MuonPi::Option { "ldap_bind_dn", &MuonPi::Config::ldap.login.bind_dn }
        << MuonPi::Option { "ldap_password", &MuonPi::Config::ldap.login.password };
    if (!credentials.read()) {
        std::cout << "Could not read credentials file.\n";
        return 1;
    }

    MuonPi::Link::Mqtt source_mqtt_link { MuonPi::Config::source_mqtt };

    if (!source_mqtt_link.wait_for(MuonPi::Link::Mqtt::Status::Connected)) {
        return -1;
    }

    MuonPi::Link::Mqtt sink_mqtt_link { MuonPi::Config::sink_mqtt };
    if (!sink_mqtt_link.wait_for(MuonPi::Link::Mqtt::Status::Connected)) {
        return -1;
    }

    MuonPi::Sink::Mqtt<MuonPi::Trigger::Detector> trigger_sink { sink_mqtt_link.publish("muonpi/trigger") };

    struct Guard {
        SinkGuard<MuonPi::Event> event {};
        SinkGuard<MuonPi::ClusterLog> clusterlog {};
        SinkGuard<MuonPi::DetectorSummary> detectorsummary {};
        MuonPi::Link::Database* db_link { nullptr };
        ~Guard()
        {
            delete db_link;
        }
    };
    std::array<Guard, 4> guard {};

    SinkGuard<MuonPi::DetectorLog> detectorlog {};

    if (!MuonPi::Config::meta.local_cluster) {
        guard[0].db_link = new MuonPi::Link::Database { MuonPi::Config::influx };

        guard[0].event.sink = new MuonPi::Sink::Database<MuonPi::Event> { *guard[0].db_link };
        guard[0].clusterlog.sink = new MuonPi::Sink::Database<MuonPi::ClusterLog> { *guard[0].db_link };
        guard[0].detectorsummary.sink = new MuonPi::Sink::Database<MuonPi::DetectorSummary> { *guard[0].db_link };

        guard[3].event.sink = new MuonPi::Sink::Mqtt<MuonPi::Event> { sink_mqtt_link.publish("muonpi/events") };

        detectorlog.sink = new MuonPi::Sink::Database<MuonPi::DetectorLog> { *guard[0].db_link };
    } else {
        guard[0].event.sink = new MuonPi::Sink::Mqtt<MuonPi::Event> { sink_mqtt_link.publish("muonpi/l1data") };
        guard[0].clusterlog.sink = new MuonPi::Sink::Mqtt<MuonPi::ClusterLog> { sink_mqtt_link.publish("muonpi/cluster") };
        guard[0].detectorsummary.sink = new MuonPi::Sink::Mqtt<MuonPi::DetectorSummary> { sink_mqtt_link.publish("muonpi/cluster") };
        detectorlog.sink = new MuonPi::Sink::Mqtt<MuonPi::DetectorLog> { sink_mqtt_link.publish("muonpi/log/") };

        reinterpret_cast<MuonPi::Sink::Mqtt<MuonPi::Event>*>(guard[0].event.sink)->set_detailed();
    }

    if (parameters["d"]) {
        guard[1].event.sink = new MuonPi::Sink::Ascii<MuonPi::Event> { std::cout };
        guard[1].clusterlog.sink = new MuonPi::Sink::Ascii<MuonPi::ClusterLog> { std::cout };
        guard[1].detectorsummary.sink = new MuonPi::Sink::Ascii<MuonPi::DetectorSummary> { std::cout };

        if (!MuonPi::Config::meta.local_cluster) {
            guard[2].event.sink = new MuonPi::Sink::Collection<MuonPi::Event, 3> { { guard[1].event.sink, guard[0].event.sink, guard[3].event.sink } };
        } else {
            guard[2].event.sink = new MuonPi::Sink::Collection<MuonPi::Event, 2> { { guard[1].event.sink, guard[0].event.sink } };
        }
        guard[2].clusterlog.sink = new MuonPi::Sink::Collection<MuonPi::ClusterLog, 2> { { guard[1].clusterlog.sink, guard[0].clusterlog.sink } };
        guard[2].detectorsummary.sink = new MuonPi::Sink::Collection<MuonPi::DetectorSummary, 2> { { guard[1].detectorsummary.sink, guard[0].detectorsummary.sink } };
    } else {
        if (!MuonPi::Config::meta.local_cluster) {
            guard[2].event.sink = new MuonPi::Sink::Collection<MuonPi::Event, 2> { { guard[0].event.sink, guard[3].event.sink } };
        } else {
            guard[2].event.sink = new MuonPi::Sink::Collection<MuonPi::Event, 1> { { guard[0].event.sink } };
        }
        guard[2].clusterlog.sink = new MuonPi::Sink::Collection<MuonPi::ClusterLog, 1> { { guard[0].clusterlog.sink } };
        guard[2].detectorsummary.sink = new MuonPi::Sink::Collection<MuonPi::DetectorSummary, 1> { { guard[0].detectorsummary.sink } };
    }

    MuonPi::StateSupervisor supervisor { *guard[2].clusterlog.sink };
    MuonPi::coincidence_filter coincidencefilter { *guard[2].event.sink, supervisor };
    MuonPi::TimeBaseSupervisor timebase_supervisor { coincidencefilter, coincidencefilter };
    MuonPi::detector_tracker detectortracker { *guard[2].detectorsummary.sink, trigger_sink, timebase_supervisor, timebase_supervisor, supervisor };
    MuonPi::TriggerHandler trigger_handler { detectortracker, MuonPi::Config::ldap, MuonPi::Config::trigger };
    MuonPi::rest::service rest_service { MuonPi::Config::rest };

    MuonPi::Source::Mqtt<MuonPi::Event> event_source { detectortracker, source_mqtt_link.subscribe("muonpi/data/#") };
    MuonPi::Source::Mqtt<MuonPi::Event> l1_source { detectortracker, source_mqtt_link.subscribe("muonpi/l1data/#") };
    MuonPi::Source::Mqtt<MuonPi::DetectorInfo<MuonPi::Location>> detector_location_source { detectortracker, source_mqtt_link.subscribe("muonpi/log/#") };

    MuonPi::Source::Mqtt<MuonPi::DetectorLog> detectorlog_source { *detectorlog.sink, source_mqtt_link.subscribe("muonpi/log/#") };

    rest_service.add_handler(&trigger_handler);

    supervisor.add_thread(&detectortracker);
    supervisor.add_thread(&sink_mqtt_link);
    supervisor.add_thread(&source_mqtt_link);
    supervisor.add_thread(&rest_service);
    supervisor.add_thread(dynamic_cast<MuonPi::Sink::Threaded<MuonPi::Event>*>(guard[2].event.sink));
    supervisor.add_thread(dynamic_cast<MuonPi::Sink::Threaded<MuonPi::DetectorSummary>*>(guard[2].detectorsummary.sink));
    supervisor.add_thread(dynamic_cast<MuonPi::Sink::Threaded<MuonPi::ClusterLog>*>(guard[2].clusterlog.sink));

    shutdown_handler = [&](int signal) {
        if (
            (signal == SIGINT)
            || (signal == SIGTERM)
            || (signal == SIGHUP)) {
            MuonPi::Log::notice() << "Received signal: " + std::to_string(signal) + ". Exiting.";
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
