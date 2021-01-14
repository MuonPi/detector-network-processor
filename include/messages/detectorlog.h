#ifndef DETECTORLOG_H
#define DETECTORLOG_H

#include "messages/userinfo.h"

#include <queue>
#include <string>
#include <variant>

namespace MuonPi {

/**
 * @struct DetectorLogItem
 * @brief The DetectorLogItem struct
 * Cointainer to hold a single log item as parameter name and value pair
 * The values are stored as variants
*/
struct DetectorLogItem {
    std::string name {}; ///< the parameter name of the log item
    std::variant<std::string, bool, std::int_fast64_t, double, std::size_t, std::uint8_t, std::uint16_t, std::uint32_t> value;
        ///< the parameter value of the log item
    std::string unit {}; ///< the unit string of the log item
    /**
     * @brief the == operator of the log item which asserts equality by comparing the name and value fields of two log items
     * @param other the log item to compare
	 * @return true on equality of name and value fields of the supplied log item and this one
     */
    bool operator==(DetectorLogItem other) { return (name == other.name && value == other.value); }
};

/**
 * @class DetectorLog
 * @brief Holds a multiple of DetectorLogItems together with UserInfo and logId fields.
*/
class DetectorLog {
public:
    /**
     * @brief add an item of type DetectorLogItem to the DetectorLog object
     * @param item the item to add of type DetectorLogItem
    */
    void add_item(DetectorLogItem item);
    /**
     * @brief Checks, if at least one item is available
     * @return true, if the object contains at least one DetectorLogItem
    */
    auto has_items() const -> bool;
    /**
     * @brief Retrieves one DetectorLogItem from the object
     * @return The DetectorLogItem object
	 * @note The retrieved item will be deleted from the object
    */
    auto next_item() -> DetectorLogItem;
    /**
     * @brief Gets the log identifier string of the log. This may be a timestamp or uuid string
	 * to identify all log items that belong to the same log epoch.
     * @return the log identification string
    */
    auto log_id() const -> const std::string&;
    /**
     * @brief Gets the user info struct of the log. The user info identifies all log messages which
	 * belong to the same source (user).
     * @return the user info struct
    */
    auto user_info() const -> UserInfo;
    /**
     * @brief Sets the user info struct of the log. The user info identifies all log messages which
	 * belong to the same source (user).
     * @param user_info the user info struct
    */
    void set_userinfo(UserInfo user_info);
    /**
     * @brief Sets the log identifier string of the log. This may be a timestamp or uuid string
	 * to identify all log items that belong to the same log epoch.
     * @param log_id the log identification string
    */
    void set_log_id(const std::string& log_id);

private:
    UserInfo m_userinfo {};
    std::string m_log_id {};
    std::queue<DetectorLogItem> m_items {};
};

}

#endif // DETECTORLOG_H
