#ifndef MUONDETECTOR_VERSION_H
#define MUONDETECTOR_VERSION_H

#include <chrono>
#include <string>
#include <memory>

#define BOOST_ENABLE_ASSERT_DEBUG_HANDLER
#cmakedefine CLUSTER_DISABLE_SSL

namespace muonpi::Version {
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
};

namespace Default {
static const ConfigFiles files {"/etc/muondetector/muondetector-cluster.cfg", "/var/muondetector/muondetector-cluster.state"};

static const Mqtt mqtt{"", 1883, {}};
static const Influx influx{"", {"", ""}, ""};
static const Ldap ldap{"ldaps://muonpi.org", {"", ""}};
static const Rest rest{1983, "0.0.0.0", "file://", "file://", "file://"};
static const Trigger trigger{"/var/muondetector/cluster_trigger"};
static const Interval interval {std::chrono::seconds{60}, std::chrono::seconds{120}, std::chrono::hours{24}};
static const Meta meta {false, 6, "muondetector_cluster" };
}

}

namespace muonpi {
class config {
public:
    [[nodiscard]] static auto singleton() -> std::shared_ptr<config>;

    Config::Mqtt source_mqtt { Config::Default::mqtt };
    Config::Mqtt sink_mqtt { Config::Default::mqtt };
    Config::Influx influx { Config::Default::influx };
    Config::Ldap ldap { Config::Default::ldap };
    Config::Rest rest { Config::Default::rest };
    Config::Trigger trigger { Config::Default::trigger };
    Config::Interval interval { Config::Default::interval };
    Config::ConfigFiles files { Config::Default::files };
    Config::Meta meta { Config::Default::meta };

private:
    static std::shared_ptr<config> s_singleton;
};
}

#endif // MUONDETECTOR_VERSION_H
