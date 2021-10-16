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
        std::string value_s {};
        int value_i {};
        double value_d {};
        ///< the parameter value of the log item
        std::string unit {}; ///< the unit string of the log item

        enum class Type {
            String
            , Int
            , Double
        } type;

        template <typename T,
                  std::enable_if_t<std::disjunction<
                                         std::is_same<T, std::string>
                                       , std::is_same<T, int>
                                       , std::is_same<T, double>
                                       >::value, bool> = true>
        auto get() const -> T
        {
            if constexpr (std::is_same_v<T, std::string>) {
                return value_s;
            } else if constexpr (std::is_same_v<T, int>) {
                return value_i;
            } else if constexpr (std::is_same_v<T, double>) {
                return value_d;
            }
        }

        item(std::string n, std::string value, std::string u)
            : name { std::move(n) }
            , value_s { std::move(value) }
            , unit { std::move(u) }
            , type { Type::String }
        {}

        item(std::string n, int value, std::string u)
            : name { std::move(n) }
            , value_i { std::move(value) }
            , unit { std::move(u) }
            , type { Type::Int }
        {}

        item(std::string n, double value, std::string u)
            : name { std::move(n) }
            , value_d { std::move(value) }
            , unit { std::move(u) }
            , type { Type::Double }
        {}

    };
    /**
     * @brief add an item of type detector_log_item to the detector_log_t object
     * @param item the item to add of type detector_log_item
    */
    void emplace(item it);

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
