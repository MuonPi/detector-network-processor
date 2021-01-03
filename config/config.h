#ifndef MUONDETECTOR_VERSION_H
#define MUONDETECTOR_VERSION_H

#include <chrono>
#include <string>

#cmakedefine CLUSTER_RUN_SERVER
#cmakedefine CLUSTER_RUN_SERVICE

namespace MuonPi::CMake::Version {
constexpr int major { @PROJECT_VERSION_MAJOR@ };
constexpr int minor { @PROJECT_VERSION_MINOR@ };
constexpr int patch { @PROJECT_VERSION_PATCH@ };

auto string() -> std::string;

}

namespace MuonPi::Config {
namespace Interval {
constexpr std::chrono::steady_clock::duration clusterlog_interval { std::chrono::seconds{60} };
constexpr std::chrono::steady_clock::duration detectorsummary_interval { std::chrono::seconds{120} };
}

constexpr struct Mqtt {
    const char* host { "" };
    int port { 1883 };
    struct Login {
        const char* username { "muonuser" };
        const char* password { "12345" };
        const char* station_id { "cluster1" };
    } login{};
} mqtt{};

constexpr struct Influx {
    const char* host { "localhost" };
    struct Login {
        const char* username { "admin" };
        const char* password { "Getdata" };
    } login{};
    const char* database { "test_cluster_db" };
} influx;

constexpr struct Ldap {
    const char* server {"ldaps://muonpi.org"};
} ldap;
constexpr struct Rest {
    std::uint16_t port { 1983 };
    const char* save_file { "/var/muondetector/cluster_trigger" };
    const char* cert { "file://" };
    const char* privkey { "file://" };
    const char* fullchain { "file://" };
} rest;
}

#endif // MUONDETECTOR_VERSION_H
