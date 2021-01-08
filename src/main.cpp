#include "utility/log.h"
#include "utility/parameters.h"
#include "utility/configuration.h"
#include "coincidencefilter.h"
#include "detectortracker.h"
#include "defaults.h"
#include "triggerhandler.h"

#include "source/mqtt.h"
#include "link/mqtt.h"
#include "sink/mqtt.h"
#include "supervision/state.h"
#include "supervision/timebase.h"


#ifdef CLUSTER_RUN_SERVER
#include "sink/database.h"
#include "link/database.h"
#else
#endif

#include "sink/ascii.h"

#include "messages/detectorsummary.h"
#include "messages/clusterlog.h"

#include <csignal>
#include <functional>

void signal_handler(int signal);


static std::function<void(int)> shutdown_handler;

void signal_handler(int signal)
{
    shutdown_handler(signal);
}

auto main(int argc, char* argv[]) -> int
{

    MuonPi::Log::Log::singleton()->add_sink(std::make_shared<MuonPi::Log::SyslogSink>());


    MuonPi::Parameters parameters{"muondetector-custer", "Calculate coincidences for the MuonPi network"};

    parameters
            <<MuonPi::Parameters::Definition{"c", "config", "Specify a configuration file to use", true}
            <<MuonPi::Parameters::Definition{"l", "credentials", "Specify a credentials file to use", true}
            <<MuonPi::Parameters::Definition{"s", "setup", "Setup the Credentials file from a plaintext file given with this option. The file will be written to the location given in the -l parameter in an encrypted format.", true}
            <<MuonPi::Parameters::Definition{"d", "debug", "Additionally to the normal sinks use Ascii sinks for debugging. Also enables the log output to stderr."};

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
            std::cout<<"No credentials location given, using default.\n";
        }

        MuonPi::Configuration credentials{parameters["s"].value};
        credentials
                <<MuonPi::Option{"source_mqtt_user", &MuonPi::Config::source_mqtt.login.username}
                <<MuonPi::Option{"source_mqtt_password", &MuonPi::Config::source_mqtt.login.password}
                <<MuonPi::Option{"source_mqtt_station_id", &MuonPi::Config::source_mqtt.login.station_id}

                <<MuonPi::Option{"sink_mqtt_user", &MuonPi::Config::sink_mqtt.login.username}
                <<MuonPi::Option{"sink_mqtt_password", &MuonPi::Config::sink_mqtt.login.password}
                <<MuonPi::Option{"sink_mqtt_station_id", &MuonPi::Config::sink_mqtt.login.station_id}

                <<MuonPi::Option{"influx_user", &MuonPi::Config::influx.login.username}
                <<MuonPi::Option{"influx_password", &MuonPi::Config::influx.login.password}
                <<MuonPi::Option{"influx_database", &MuonPi::Config::influx.database}

                <<MuonPi::Option{"ldap_bind_dn", &MuonPi::Config::ldap.login.bind_dn}
                <<MuonPi::Option{"ldap_password", &MuonPi::Config::ldap.login.password}
                  ;
        if (!credentials.read()) {
            std::cout<<"Could not read input file.\n";
            return 1;
        }

        credentials.set_encrypted(true);
        credentials.set_filename(MuonPi::Config::files.credentials);

        if (credentials.write()) {
            std::cout<<"Wrote credentials file.\n";
            return 0;
        }
        std::cout<<"Could not write credentials file.\n";
        return 1;
    }

    MuonPi::Configuration config{MuonPi::Config::files.config};
    config
            <<MuonPi::Option{"source_mqtt_host", &MuonPi::Config::source_mqtt.host}
            <<MuonPi::Option{"source_mqtt_port", &MuonPi::Config::source_mqtt.port}

            <<MuonPi::Option{"sink_mqtt_host", &MuonPi::Config::sink_mqtt.host}
            <<MuonPi::Option{"sink_mqtt_port", &MuonPi::Config::sink_mqtt.port}

            <<MuonPi::Option{"influx_host", &MuonPi::Config::influx.host}
            <<MuonPi::Option{"influx_cluster_id", &MuonPi::Config::influx.cluster_id}

            <<MuonPi::Option{"ldap_host", &MuonPi::Config::ldap.server}

            <<MuonPi::Option{"rest_port", &MuonPi::Config::rest.port}
            <<MuonPi::Option{"rest_trigger_file", &MuonPi::Config::rest.save_file}
            <<MuonPi::Option{"rest_cert", &MuonPi::Config::rest.cert}
            <<MuonPi::Option{"rest_privkey", &MuonPi::Config::rest.privkey}
            <<MuonPi::Option{"rest_fullchain", &MuonPi::Config::rest.fullchain}

            <<MuonPi::Option{"run_local_cluster", &MuonPi::Config::meta.local_cluster}
           ;
    if (!parameters["l"]) {
        config<<MuonPi::Option{"credentials_file", &MuonPi::Config::files.credentials};
    }
    if (!config.read()) {
        std::cout<<"Could not read configuration file.\n";
        return 1;
    }

    MuonPi::Configuration credentials{MuonPi::Config::files.credentials, true};

    credentials
            <<MuonPi::Option{"source_mqtt_user", &MuonPi::Config::source_mqtt.login.username}
            <<MuonPi::Option{"source_mqtt_password", &MuonPi::Config::source_mqtt.login.password}
            <<MuonPi::Option{"source_mqtt_station_id", &MuonPi::Config::source_mqtt.login.station_id}

            <<MuonPi::Option{"sink_mqtt_user", &MuonPi::Config::sink_mqtt.login.username}
            <<MuonPi::Option{"sink_mqtt_password", &MuonPi::Config::sink_mqtt.login.password}
            <<MuonPi::Option{"sink_mqtt_station_id", &MuonPi::Config::sink_mqtt.login.station_id}

            <<MuonPi::Option{"influx_user", &MuonPi::Config::influx.login.username}
            <<MuonPi::Option{"influx_password", &MuonPi::Config::influx.login.password}
            <<MuonPi::Option{"influx_database", &MuonPi::Config::influx.database}

            <<MuonPi::Option{"ldap_bind_dn", &MuonPi::Config::ldap.login.bind_dn}
            <<MuonPi::Option{"ldap_password", &MuonPi::Config::ldap.login.password}
              ;
    if (!credentials.read()) {
        std::cout<<"Could not read credentials file.\n";
        return 1;
    }


    MuonPi::Link::Mqtt source_mqtt_link {MuonPi::Config::source_mqtt};

    if (!source_mqtt_link.wait_for(MuonPi::Link::Mqtt::Status::Connected)) {
        return -1;
    }

    MuonPi::Link::Mqtt sink_mqtt_link {MuonPi::Config::sink_mqtt};
    if (!sink_mqtt_link.wait_for(MuonPi::Link::Mqtt::Status::Connected)) {
        return -1;
    }

    MuonPi::Sink::Mqtt<MuonPi::Trigger::Detector> trigger_sink {sink_mqtt_link.publish("muonpi/trigger")};

    std::unique_ptr<MuonPi::Sink::Base<MuonPi::Event>> event_sink { nullptr };
    std::unique_ptr<MuonPi::Sink::Base<MuonPi::Event>> clusterlog_sink { nullptr };
    std::unique_ptr<MuonPi::Sink::Base<MuonPi::Event>> detectorlog_sink { nullptr };
    std::unique_ptr<MuonPi::Link::Database> db_link { nullptr };
    struct Guard {
    MuonPi::Sink::Base<MuonPi::Event>* event_sink { nullptr };
    MuonPi::Sink::Base<MuonPi::ClusterLog>* clusterlog_sink { nullptr };
    MuonPi::Sink::Base<MuonPi::DetectorSummary>* detectorlog_sink { nullptr };
    MuonPi::Link::Database* db_link { nullptr };
    ~Guard() {
        delete event_sink;
        delete clusterlog_sink;
        delete detectorlog_sink;
        delete db_link;
    }
    };
    std::array<Guard, 3> guard{};



    if (!MuonPi::Config::meta.local_cluster) {
        guard[0].db_link = new MuonPi::Link::Database {MuonPi::Config::influx};

        guard[0].event_sink = new MuonPi::Sink::Database<MuonPi::Event>{ *guard[0].db_link };
        guard[0].clusterlog_sink = new MuonPi::Sink::Database<MuonPi::ClusterLog>{ *guard[0].db_link };
        guard[0].detectorlog_sink = new MuonPi::Sink::Database<MuonPi::DetectorSummary>{ *guard[0].db_link };
    } else {
        guard[0].event_sink = new MuonPi::Sink::Mqtt<MuonPi::Event>{sink_mqtt_link.publish("muonpi/l1data")};
        guard[0].clusterlog_sink = new MuonPi::Sink::Mqtt<MuonPi::ClusterLog>{sink_mqtt_link.publish("muonpi/cluster")};
        guard[0].detectorlog_sink = new MuonPi::Sink::Mqtt<MuonPi::DetectorSummary>{sink_mqtt_link.publish("muonpi/cluster")};
    }

    MuonPi::Sink::Mqtt<MuonPi::Event> mqtt_broadcast_sink { sink_mqtt_link.publish("muonpi/events") };


    if (parameters["d"]) {
        guard[1].event_sink = new MuonPi::Sink::Ascii<MuonPi::Event>{ std::cout };
        guard[1].clusterlog_sink = new MuonPi::Sink::Ascii<MuonPi::ClusterLog>{ std::cout };
        guard[1].detectorlog_sink = new MuonPi::Sink::Ascii<MuonPi::DetectorSummary>{ std::cout };

        guard[2].event_sink = new MuonPi::Sink::Collection<MuonPi::Event, 3>{{guard[1].event_sink, guard[0].event_sink, &mqtt_broadcast_sink}};
        guard[2].clusterlog_sink = new MuonPi::Sink::Collection<MuonPi::ClusterLog, 2>{{guard[1].clusterlog_sink, guard[0].clusterlog_sink}};
        guard[2].detectorlog_sink = new MuonPi::Sink::Collection<MuonPi::DetectorSummary, 2>{{guard[1].detectorlog_sink, guard[0].detectorlog_sink}};
    } else {
        guard[2].event_sink = new MuonPi::Sink::Collection<MuonPi::Event, 2>{{guard[0].event_sink, &mqtt_broadcast_sink}};
        guard[2].clusterlog_sink = new MuonPi::Sink::Collection<MuonPi::ClusterLog, 1>{{guard[0].clusterlog_sink}};
        guard[2].detectorlog_sink = new MuonPi::Sink::Collection<MuonPi::DetectorSummary, 1>{{guard[0].detectorlog_sink}};
    }


    MuonPi::StateSupervisor supervisor{*guard[2].clusterlog_sink};
    MuonPi::CoincidenceFilter coincidence_filter{*guard[2].event_sink, supervisor};
    MuonPi::TimeBaseSupervisor timebase_supervisor{coincidence_filter, coincidence_filter};
    MuonPi::DetectorTracker detector_tracker{*guard[2].detectorlog_sink, trigger_sink, timebase_supervisor, timebase_supervisor, supervisor};
    MuonPi::TriggerHandler trigger_handler{detector_tracker, MuonPi::Config::rest, MuonPi::Config::ldap};


    MuonPi::Source::Mqtt<MuonPi::Event> event_source { detector_tracker, source_mqtt_link.subscribe("muonpi/data/#") };
    MuonPi::Source::Mqtt<MuonPi::DetectorInfo<MuonPi::Location>> detector_location_source { detector_tracker, source_mqtt_link.subscribe("muonpi/log/#") };

    supervisor.add_thread(&detector_tracker);
    supervisor.add_thread(&sink_mqtt_link);
    supervisor.add_thread(&source_mqtt_link);
    supervisor.add_thread(dynamic_cast<MuonPi::Sink::Threaded<MuonPi::Event>*>(guard[2].event_sink));
    supervisor.add_thread(dynamic_cast<MuonPi::Sink::Threaded<MuonPi::DetectorSummary>*>(guard[2].detectorlog_sink));
    supervisor.add_thread(dynamic_cast<MuonPi::Sink::Threaded<MuonPi::ClusterLog>*>(guard[2].clusterlog_sink));


    shutdown_handler = [&](int signal) {
        if (
                   (signal == SIGINT)
                || (signal == SIGTERM)
                || (signal == SIGHUP)
                ) {
            MuonPi::Log::notice()<<"Received signal: " + std::to_string(signal) + ". Exiting.";
            supervisor.stop();
            coincidence_filter.stop();
        }
    };

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGHUP, signal_handler);

    coincidence_filter.start_synchronuos();

    return coincidence_filter.wait();
}
