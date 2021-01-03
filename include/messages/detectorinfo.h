#ifndef DETECTORINFO_H
#define DETECTORINFO_H

#include "messages/userinfo.h"

#include <chrono>
#include <string>

namespace MuonPi {


struct DetectorType {
	enum class GateConnection {
		NONE, XOR, AND, SINGLE
	} detector_gate;
	enum class DetectorCount {
		NONE, SINGLE, DOUBLE
	} detector_count;
	enum class PhysicalType {
		UNDEFINED, SCINTILLATOR, SEMICONDUCTOR, OTHER
	} physical_type;
	double cross_section { 0.0 };
	[[nodiscard]] auto id() const -> std::uint8_t;
};

enum class DetectorState {
	UNDEFINED, INACTIVE, ACTIVE
};

/**
 * @brief The DetectorInfo class
 */
class DetectorInfo
{
public:

	struct Location {
		double lat { 0.0 };
		double lon { 0.0 };
		double h { 0.0 };
		double v_acc { 0.0 };
		double h_acc { 0.0 };
		double dop { 0.0 };
	};
	struct Time
	{
		double accuracy { 0.0 };
		double dop { 0.0 };
	};
	
	DetectorState state { DetectorState::UNDEFINED };
	
	struct VersionInfo 
	{
		std::string hw_version { "" };
		std::string sw_version { "" };
		std::string ublox_hw_version { "" };
		std::string ublox_sw_version { "" };
		std::string ublox_proto_version { "" };
	};

	DetectorType type { DetectorType::GateConnection::NONE, DetectorType::DetectorCount::NONE, DetectorType::PhysicalType::UNDEFINED, 0.0 };
	
	/**
     * @brief DetectorInfo
     * @param hash The hash of the detector identifier
     * @param user_info The user info object
     * @param location The detector location information
     */
    DetectorInfo(std::size_t hash, UserInfo user_info, Location location);

    DetectorInfo() noexcept;

    /**
     * @brief hash
     * @return The hash of the detector for this event
     */
    [[nodiscard]] auto hash() const noexcept -> std::size_t;

    /**
     * @brief location The location of the detector from this log message
     * @return The location data
     */
    [[nodiscard]] auto location() const -> Location;

    /**
     * @brief time The time this log message arrived
     * @return The arrival time
     */
    [[nodiscard]] auto time() const -> std::chrono::system_clock::time_point;

//    [[nodiscard]] auto valid() const -> bool;

    /**
     * @brief data Accesses the user info from the object
     * @return the UserInfo struct
     */
    [[nodiscard]] auto user_info() const -> UserInfo;


    std::size_t m_hash { 0 };
    Location m_location {};
    Time m_time_info {};
    UserInfo m_userinfo {};


private:
    std::chrono::system_clock::time_point m_time { std::chrono::system_clock::now() };

    bool m_valid { true };
};
}

#endif // DETECTORINFO_H
