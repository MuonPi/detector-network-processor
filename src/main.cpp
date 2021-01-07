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

#ifndef CLUSTER_RUN_SERVICE
    MuonPi::Log::Log::singleton()->add_sink(std::make_shared<MuonPi::Log::StreamSink>(std::cerr));
#endif

    MuonPi::Log::Log::singleton()->add_sink(std::make_shared<MuonPi::Log::SyslogSink>());


    MuonPi::Parameters parameters{"muondetector-custer", "Calculate coincidences for the MuonPi network"};

    parameters
            <<MuonPi::Parameters::Definition{"c", "config", "Specify a configuration file to use", true}
            <<MuonPi::Parameters::Definition{"l", "credentials", "Specify a credentials file to use", true}
            <<MuonPi::Parameters::Definition{"s", "setup", "Setup the Credentials file from a plaintext file given with this option. The file will be written to the location given in the -l parameter in an encrypted format.", true};

    if (!parameters.start(argc, argv)) {
        return 0;
    }
    if (parameters["l"]) {
        MuonPi::Config::files.credentials = parameters["l"].value.c_str();
    }
    if (parameters["c"]) {
        MuonPi::Config::files.config = parameters["c"].value.c_str();
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

#ifdef CLUSTER_RUN_SERVER
    MuonPi::Link::Database db_link {};

    MuonPi::Sink::Database<MuonPi::Event> event_sink { db_link };
    MuonPi::Sink::Database<MuonPi::ClusterLog> clusterlog_sink { db_link };
    MuonPi::Sink::Database<MuonPi::DetectorSummary> detectorlog_sink { db_link };


#else

    MuonPi::Sink::Mqtt<MuonPi::Event> event_sink {sink_mqtt_link.publish("muonpi/l1data")};
    MuonPi::Sink::Mqtt<MuonPi::ClusterLog> clusterlog_sink {sink_mqtt_link.publish("muonpi/cluster")};
    MuonPi::Sink::Mqtt<MuonPi::DetectorSummary> detectorlog_sink {sink_mqtt_link.publish("muonpi/cluster")};
#endif

    MuonPi::Sink::Mqtt<MuonPi::Event> mqtt_broadcast_sink { sink_mqtt_link.publish("muonpi/events") };


#ifndef CLUSTER_RUN_SERVICE
    MuonPi::Sink::Ascii<MuonPi::ClusterLog> ascii_clusterlog_sink { std::cout };
    MuonPi::Sink::Ascii<MuonPi::DetectorSummary> ascii_detectorlog_sink { std::cout };
    MuonPi::Sink::Ascii<MuonPi::Event> ascii_event_sink { std::cout };

    MuonPi::Sink::Collection<MuonPi::ClusterLog, 2> cluster_sinks {{&ascii_clusterlog_sink, &clusterlog_sink}};
    MuonPi::Sink::Collection<MuonPi::DetectorSummary, 2> detector_sinks {{&ascii_detectorlog_sink, &detectorlog_sink}};
    MuonPi::Sink::Collection<MuonPi::Event, 3> event_sinks {{&ascii_event_sink, &event_sink, &mqtt_broadcast_sink}};
#else
    MuonPi::Sink::Collection<MuonPi::ClusterLog, 1> cluster_sinks {{&clusterlog_sink}};
    MuonPi::Sink::Collection<MuonPi::DetectorSummary, 1> detector_sinks {{&detectorlog_sink}};
    MuonPi::Sink::Collection<MuonPi::Event, 2> event_sinks {{&event_sink, &mqtt_broadcast_sink}};
#endif


    MuonPi::StateSupervisor supervisor{cluster_sinks};
    MuonPi::DetectorTracker detector_tracker{detector_sinks, trigger_sink, supervisor};
    MuonPi::CoincidenceFilter coincidence_filter{event_sinks, detector_tracker, supervisor};
    MuonPi::TriggerHandler trigger_handler{detector_tracker};


    MuonPi::Source::Mqtt<MuonPi::Event> event_source { detector_tracker, source_mqtt_link.subscribe("muonpi/data/#") };
    MuonPi::Source::Mqtt<MuonPi::DetectorInfo<MuonPi::Location>> detector_location_source { detector_tracker, source_mqtt_link.subscribe("muonpi/log/#") };

    supervisor.add_thread(&detector_tracker);
    supervisor.add_thread(&sink_mqtt_link);
    supervisor.add_thread(&source_mqtt_link);
    supervisor.add_thread(&event_sinks);
    supervisor.add_thread(&detector_sinks);
    supervisor.add_thread(&cluster_sinks);


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
