#ifndef DETECTORLOG_H
#define DETECTORLOG_H

#include "messages/userinfo.h"

#include <queue>
#include <string>
#include <variant>

namespace muonpi {

struct detector_log_t {
    struct item {
        std::string name {}; ///< the parameter name of the log item
        std::variant<std::string, bool, std::int_fast64_t, double, std::size_t, std::uint8_t, std::uint16_t, std::uint32_t> value;
        ///< the parameter value of the log item
        std::string unit {}; ///< the unit string of the log item
    };
    /**
     * @brief add an item of type detector_log_item to the detector_log_t object
     * @param item the item to add of type detector_log_item
    */
    void emplace(item item);

    /**
     * @brief Retrieves one detector_log_item from the object
     * @return The detector_log_item object
	 * @note The retrieved item will be deleted from the object
    */
    auto get() -> item;

    userinfo_t userinfo {};
    std::string log_id {};
    std::queue<item> items {};
};

}

#endif // DETECTORLOG_H
