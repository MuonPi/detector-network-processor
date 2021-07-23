#ifndef MUONDETECTOR_VERSION_H
#define MUONDETECTOR_VERSION_H

#include <muonpi/configuration.h>

#include <chrono>
#include <string>
#include <memory>
#include <optional>

#define BOOST_ENABLE_ASSERT_DEBUG_HANDLER
#cmakedefine PROCESSOR_DISABLE_SSL

namespace muonpi::Version::dnp {
constexpr int major { @PROJECT_VERSION_MAJOR@ };
constexpr int minor { @PROJECT_VERSION_MINOR@ };
constexpr int patch { @PROJECT_VERSION_PATCH@ };
constexpr const char* additional { "@PROJECT_VERSION_ADDITIONAL@" };

[[nodiscard]] auto string() -> std::string;

}

namespace muonpi::Config {


struct Interval {
    std::chrono::steady_clock::duration clusterlog {};
    std::chrono::steady_clock::duration detectorsummary {};
    std::chrono::system_clock::duration histogram_sample_time {};
};

struct Mqtt {
    std::string host {};
    int port {};
    struct Login {
        std::string username {};
        std::string password {};
    } login;
};

struct Influx {
    std::string host {};
    struct Login {
        std::string username {};
        std::string password {};
    } login;
    std::string database {};
};

struct Ldap {
    std::string host {};
    struct Login {
        std::string bind_dn {};
        std::string password {};
    } login;
};

struct Trigger {
    std::string save_file {};
};

struct Rest {
    int port {};
    std::string address {};
    std::string cert {};
    std::string privkey {};
    std::string fullchain {};
};
struct ConfigFiles {
    std::string config {};
    std::string state {};
};

struct Meta {
    bool local_cluster {};
    int max_geohash_length {};
    std::string station {};
    int verbosity {};
};

namespace Default {
static const ConfigFiles files {"/etc/muondetector/detector-network-processor.cfg", "/var/muondetector/detector-network-processor.state"};

static const Mqtt mqtt{"", 1883, {}};
static const Influx influx{"", {"", ""}, ""};
static const Ldap ldap{"ldaps://muonpi.org", {"", ""}};
static const Rest rest{1983, "0.0.0.0", "file://", "file://", "file://"};
static const Trigger trigger{"/var/muondetector/cluster_trigger"};
static const Interval interval {std::chrono::seconds{60}, std::chrono::seconds{120}, std::chrono::hours{24}};
static const Meta meta {false, 6, "muondetector_cluster", 0};
}

[[nodiscard]] auto setup(int argc, const char* argv[]) -> std::optional<config>;
}

#endif // MUONDETECTOR_VERSION_H
