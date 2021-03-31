#ifndef DETECTORLOG_H
#define DETECTORLOG_H

#include "messages/userinfo.h"

#include <queue>
#include <string>
#include <variant>

namespace muonpi {

/**
 * @struct detector_log_item
 * @brief The detector_log_item struct
 * Cointainer to hold a single log item as parameter name and value pair
 * The values are stored as variants
*/
struct detector_log_item {
    std::string name {}; ///< the parameter name of the log item
    std::variant<std::string, bool, std::int_fast64_t, double, std::size_t, std::uint8_t, std::uint16_t, std::uint32_t> value;
    ///< the parameter value of the log item
    std::string unit {}; ///< the unit string of the log item
    /**
     * @brief the == operator of the log item which asserts equality by comparing the name and value fields of two log items
     * @param other the log item to compare
	 * @return true on equality of name and value fields of the supplied log item and this one
     */
    bool operator==(detector_log_item other) { return (name == other.name && value == other.value); }
};

/**
 * @class detector_log_t
 * @brief Holds a multiple of detector_log_items together with userinfo_t and logId fields.
*/
class detector_log_t {
public:
    /**
     * @brief add an item of type detector_log_item to the detector_log_t object
     * @param item the item to add of type detector_log_item
    */
    void add_item(detector_log_item item);
    /**
     * @brief Checks, if at least one item is available
     * @return true, if the object contains at least one detector_log_item
    */
    auto has_items() const -> bool;
    /**
     * @brief Retrieves one detector_log_item from the object
     * @return The detector_log_item object
	 * @note The retrieved item will be deleted from the object
    */
    auto next_item() -> detector_log_item;
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
    auto user_info() const -> userinfo_t;
    /**
     * @brief Sets the user info struct of the log. The user info identifies all log messages which
	 * belong to the same source (user).
     * @param user_info the user info struct
    */
    void set_userinfo(userinfo_t user_info);
    /**
     * @brief Sets the log identifier string of the log. This may be a timestamp or uuid string
	 * to identify all log items that belong to the same log epoch.
     * @param log_id the log identification string
    */
    void set_log_id(const std::string& log_id);

private:
    userinfo_t m_userinfo {};
    std::string m_log_id {};
    std::queue<detector_log_item> m_items {};
};

}

#endif // DETECTORLOG_H
