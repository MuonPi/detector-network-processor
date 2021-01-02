#ifndef MUONDETECTOR_VERSION_H
#define MUONDETECTOR_VERSION_H

#include <chrono>

#cmakedefine CLUSTER_RUN_SERVER
#cmakedefine CLUSTER_RUN_SERVICE

namespace MuonPi::CMake::Version {
constexpr int major { @PROJECT_VERSION_MAJOR@ };
constexpr int minor { @PROJECT_VERSION_MINOR@ };
constexpr int patch { @PROJECT_VERSION_PATCH@ };
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
        const char* username { "" };
        const char* password { "" };
        const char* station_id { "" };
    } login{};
} mqtt{};

constexpr struct Influx {
    const char* host { "" };
    struct Login {
        const char* username { "" };
        const char* password { "" };
    } login{};
    const char* database { "" };
} influx;

constexpr struct Ldap {
    const char* server {"ldaps://muonpi.org"};
} ldap;
constexpr struct Rest {
    std::uint16_t port { 1983 };
} rest;
}

#endif // MUONDETECTOR_VERSION_H
