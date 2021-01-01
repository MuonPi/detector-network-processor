#include "utility/log.h"
#include "core.h"
#include "detectortracker.h"

#include "source/mqtt.h"
#include "link/mqtt.h"
#include "sink/mqtt.h"
#include "supervision/state.h"

#define CLUSTER_RUN_SERVER

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

auto main() -> int
{
    MuonPi::Log::Log::singleton()->add_sink(std::make_shared<MuonPi::Log::StreamSink>(std::cerr));
//    MuonPi::Log::Log::singleton()->add_sink(std::make_shared<MuonPi::Log::SyslogSink>());


    MuonPi::Link::Mqtt::LoginData login;

    login.username = "";
    login.password = "";
    login.station_id = "";

    MuonPi::Link::Mqtt mqtt_link {login, "116.202.96.181", 1883};

    if (!mqtt_link.wait_for(MuonPi::Link::Mqtt::Status::Connected)) {
        return -1;
    }


    MuonPi::Sink::Ascii<MuonPi::ClusterLog> ascii_clusterlog_sink { std::cout };
    MuonPi::Sink::Ascii<MuonPi::DetectorSummary> ascii_detectorlog_sink { std::cout };
    MuonPi::Sink::Ascii<MuonPi::Event> ascii_event_sink { std::cout };

#ifdef CLUSTER_RUN_SERVER
    MuonPi::Link::Database db_link {"", {"", ""}, ""};

    MuonPi::Sink::Database<MuonPi::Event> event_sink { db_link };
    MuonPi::Sink::Database<MuonPi::ClusterLog> clusterlog_sink { db_link };
    MuonPi::Sink::Database<MuonPi::DetectorSummary> detectorlog_sink { db_link };

#else

    auto event_sink { std::make_shared<MuonPi::MqttSink<MuonPi::Event>>(mqtt_link.publish("muonpi/l1data")) };
    auto clusterlog_sink { std::make_shared<MuonPi::MqttSink<MuonPi::ClusterLog>>(mqtt_link.publish("muonpi/cluster")) };
    auto detectorlog_sink { std::make_shared<MuonPi::MqttSink<MuonPi::DetectorSummary>>(mqtt_link.publish("muonpi/cluster")) };

#endif

//    MuonPi::Sink::Mqtt<MuonPi::Event> mqtt_broadcast_sink { mqtt_link.publish("muonpi/events") };


    MuonPi::Sink::Collection<MuonPi::ClusterLog, 2> cluster_sinks {{&ascii_clusterlog_sink, &clusterlog_sink}};
    MuonPi::Sink::Collection<MuonPi::DetectorSummary, 2> detector_sinks {{&ascii_detectorlog_sink, &detectorlog_sink}};
    MuonPi::Sink::Collection<MuonPi::Event, 2> event_sinks {{&ascii_event_sink, &event_sink/*, &mqtt_broadcast_sink*/}};

    MuonPi::StateSupervisor supervisor{cluster_sinks};
    MuonPi::DetectorTracker detector_tracker{detector_sinks, supervisor};
    MuonPi::Core core{event_sinks, detector_tracker, supervisor};

    MuonPi::Sink::Collection<MuonPi::Event, 2> incoming_sinks {{&core, &event_sinks}};

    MuonPi::Source::Mqtt<MuonPi::Event> event_source { incoming_sinks, mqtt_link.subscribe("muonpi/data/#") };
    MuonPi::Source::Mqtt<MuonPi::DetectorInfo> log_source { detector_tracker, mqtt_link.subscribe("muonpi/log/#") };

    supervisor.add_thread(&detector_tracker);
    supervisor.add_thread(&mqtt_link);
    supervisor.add_thread(&event_sinks);
    supervisor.add_thread(&detector_sinks);
    supervisor.add_thread(&cluster_sinks);
    supervisor.add_thread(&incoming_sinks);


    shutdown_handler = [&](int signal) {
        if (
                   (signal == SIGINT)
                || (signal == SIGTERM)
                || (signal == SIGHUP)
                ) {
            MuonPi::Log::notice()<<"Received signal: " + std::to_string(signal) + ". Exiting.";
            supervisor.stop();
        }
        if (
                   signal == SIGSEGV
                ) {
            MuonPi::Log::critical()<<"Received signal: " + std::to_string(signal) + ". Exiting.";
            supervisor.stop();
            core.stop();
        }
    };

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGHUP, signal_handler);
    std::signal(SIGSEGV, signal_handler);

    core.start_synchronuos();

    return core.wait();
}
