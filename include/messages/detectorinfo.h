#ifndef DETECTORINFO_H
#define DETECTORINFO_H

#include "messages/userinfo.h"

#include <chrono>
#include <string>
#include <tuple>

namespace muonpi {

struct detector_type {
    enum class GateConnection {
        NONE,
        XOR,
        AND,
        SINGLE
    } detector_gate;
    enum class detectorCount {
        NONE,
        SINGLE,
        DOUBLE
    } detector_count;
    enum class PhysicalType {
        UNDEFINED,
        SCINTILLATOR,
        SEMICONDUCTOR,
        OTHER
    } physical_type;
    double cross_section { 0.0 };
};

enum class detector_state {
    UNDEFINED,
    INACTIVE,
    ACTIVE
};

struct location_t {
    double lat { 0.0 };
    double lon { 0.0 };
    double h { 0.0 };
    double v_acc { 0.0 };
    double h_acc { 0.0 };
    double dop { 0.0 };
    std::string geohash { "" };
    std::uint8_t max_geohash_length {};
};
struct time_t {
    double accuracy { 0.0 };
    double dop { 0.0 };
};
struct version_info_t {
    std::string hw_version { "" };
    std::string sw_version { "" };
    std::string ublox_hw_version { "" };
    std::string ublox_sw_version { "" };
    std::string ublox_proto_version { "" };
};
struct bias_t {
    double bias_voltage { 0.0 };
    double bias_current { 0.0 };
};
struct thresholds_t {
    double threshold1 { 0.0 };
    double threshold2 { 0.0 };
};
struct gnss_parameters_t {
    double sats_received { 0. };
    double sats_used { 0. };
};

template <typename... T>
struct detector_info_t {
    std::tuple<T...> items {};
    std::size_t hash {};
    userinfo_t userinfo {};

    template <typename I>
    [[nodiscard]] inline auto item() -> I&
    {
        return std::get<I>(items);
    }

    template <typename I>
    [[nodiscard]] inline auto get() const -> I
    {
        return std::get<I>(items);
    }
};

}

#endif // DETECTORINFO_H
