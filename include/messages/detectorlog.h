#ifndef DETECTORLOG_H
#define DETECTORLOG_H

#include "messages/userinfo.h"

#include <chrono>
#include <string>
#include <list>
#include <variant>

namespace MuonPi {


struct DetectorLogItem
{
	std::string name {};
	std::variant<std::string, bool, std::int_fast64_t, double, std::size_t, std::uint8_t, std::uint16_t, std::uint32_t> value;
	bool operator==(DetectorLogItem other) { return (name==other.name && value==other.value); } 
};

class DetectorLog {
public:
	void add_item(DetectorLogItem item);
	auto has_items() const -> bool;
	auto next_item() -> DetectorLogItem;
	auto log_id() const -> const std::string&;
	void set_userinfo(UserInfo user_info);
	void set_log_id(const std::string& log_id);
private:	
	UserInfo m_userinfo { };
	std::string m_log_id { };
	std::list<DetectorLogItem> m_items { };
};


/*	
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
struct Location {
	double lat { 0.0 };
	double lon { 0.0 };
	double h { 0.0 };
	double v_acc { 0.0 };
	double h_acc { 0.0 };
	double dop { 0.0 };
	std::string geohash { "" };
};
struct Time
{
	double accuracy { 0.0 };
	double dop { 0.0 };
};
struct VersionInfo 
{
	std::string hw_version { "" };
	std::string sw_version { "" };
	std::string ublox_hw_version { "" };
	std::string ublox_sw_version { "" };
	std::string ublox_proto_version { "" };
};
struct Bias
{
	double bias_voltage { 0.0 };
	double bias_current { 0.0 };
};
struct Thresholds
{
	double threshold1 { 0.0 };
	double threshold2 { 0.0 };
};
struct GnssParameters
{
	double sats_received { 0. };
	double sats_used { 0. };
};

*/



}

#endif // DETECTORLOG_H
