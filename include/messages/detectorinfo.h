#ifndef DETECTORINFO_H
#define DETECTORINFO_H

#include "messages/userinfo.h"

#include <chrono>
#include <string>

namespace muonpi {

struct detectorType {
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
    [[nodiscard]] static auto id() -> std::uint8_t;
};

enum class detectorState {
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
struct Time {
    double accuracy { 0.0 };
    double dop { 0.0 };
};
struct VersionInfo {
    std::string hw_version { "" };
    std::string sw_version { "" };
    std::string ublox_hw_version { "" };
    std::string ublox_sw_version { "" };
    std::string ublox_proto_version { "" };
};
struct Bias {
    double bias_voltage { 0.0 };
    double bias_current { 0.0 };
};
struct Thresholds {
    double threshold1 { 0.0 };
    double threshold2 { 0.0 };
};
struct GnssParameters {
    double sats_received { 0. };
    double sats_used { 0. };
};

/**
 * @brief The detetor_info_t class
 */
template <typename T>
class detetor_info_t {
public:
    //	detectorType type { detectorType::GateConnection::NONE, detectorType::detectorCount::NONE, detectorType::PhysicalType::UNDEFINED, 0.0 };
    //	detectorState state { detectorState::UNDEFINED };

    /**
     * @brief detetor_info_t
     * @param hash The hash of the detector identifier
     * @param user_info The user info object
     * @param item The specific detector info struct
     */
    detetor_info_t(std::size_t hash, userinfo_t user_info, T item);

    detetor_info_t() noexcept;

    /**
     * @brief hash
     * @return The hash of the detector for this info object
     */
    [[nodiscard]] auto hash() const noexcept -> std::size_t;

    /**
     * @brief item The item stored in this detetor_info_t object
     * @return The specific detector info struct
     */
    [[nodiscard]] auto item() const -> T;

    /**
     * @brief time The time this log message arrived
     * @return The arrival time
     */
    [[nodiscard]] auto time() const -> std::chrono::system_clock::time_point;

    [[nodiscard]] auto valid() const -> bool;

    /**
     * @brief data Accesses the user info from the object
     * @return the userinfo_t struct
     */
    [[nodiscard]] auto user_info() const -> userinfo_t;

    std::size_t m_hash { 0 };
    T m_item {};
    userinfo_t m_userinfo {};

private:
    std::chrono::system_clock::time_point m_time { std::chrono::system_clock::now() };

    bool m_valid { true };
};

/*
* implementation section
*/

template <typename T>
detetor_info_t<T>::detetor_info_t(std::size_t hash, userinfo_t user_info, T item)
    : m_hash { hash }
    , m_item { item }
    , m_userinfo { user_info }
{
}

template <typename T>
detetor_info_t<T>::detetor_info_t() noexcept
    : m_valid { false }
{
}

template <typename T>
auto detetor_info_t<T>::hash() const noexcept -> std::size_t
{
    return m_hash;
}

template <typename T>
auto detetor_info_t<T>::item() const -> T
{
    return m_item;
}

template <typename T>
auto detetor_info_t<T>::user_info() const -> userinfo_t
{
    return m_userinfo;
}

template <typename T>
auto detetor_info_t<T>::time() const -> std::chrono::system_clock::time_point
{
    return m_time;
}

template <typename T>
auto detetor_info_t<T>::valid() const -> bool
{
    return m_valid;
}

}

#endif // DETECTORINFO_H
