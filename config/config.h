#ifndef MUONDETECTOR_VERSION_H
#define MUONDETECTOR_VERSION_H

#include <chrono>
#include <string>

namespace MuonPi::Version {
constexpr int major { @PROJECT_VERSION_MAJOR@ };
constexpr int minor { @PROJECT_VERSION_MINOR@ };
constexpr int patch { @PROJECT_VERSION_PATCH@ };

auto string() -> std::string;

}

namespace MuonPi::Config {


struct Interval {
    std::chrono::steady_clock::duration clusterlog {};
    std::chrono::steady_clock::duration detectorsummary {};
};

struct Mqtt {
    std::string host {};
    int port {};
    struct Login {
        std::string username {};
        std::string password {};
        std::string station_id {};
    } login;
};

struct Influx {
    std::string host {};
    struct Login {
        std::string username {};
        std::string password {};
    } login;
    std::string database {};
    std::string cluster_id {};
};

struct Ldap {
    std::string server {};
    struct Login {
        std::string bind_dn {};
        std::string password {};
    } login;
};

struct Rest {
    int port {};
    std::string save_file {};
    std::string cert {};
    std::string privkey {};
    std::string fullchain {};
};
struct ConfigFiles {
    std::string config {};
    std::string credentials {};
};

struct Meta {
    bool local_cluster {};
};

namespace Default {
static ConfigFiles files {"/etc/muondetector/muondetector-cluster.cfg", "/var/muondetector/muondetector-cluster"};

static Mqtt mqtt{"", 1883, {}};
static Influx influx{"", {"", ""}, "", ""};
static Ldap ldap{"ldaps://muonpi.org", {"", ""}};
static Rest rest{1983, "/var/muondetector/cluster_trigger", "file://", "file://", "file://"};
static Interval interval {std::chrono::seconds{60}, std::chrono::seconds{120}};
static Meta meta {false};
}


static Mqtt source_mqtt { Default::mqtt };
static Mqtt sink_mqtt { Default::mqtt };
static Influx influx { Default::influx };
static Ldap ldap { Default::ldap };
static Rest rest { Default::rest };
static Interval interval { Default::interval };
static ConfigFiles files { Default::files };
static Meta meta { Default::meta };
}

#endif // MUONDETECTOR_VERSION_H
