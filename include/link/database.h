#ifndef DATABASELINK_H
#define DATABASELINK_H

#include "defaults.h"

#include <mutex>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

namespace muonpi::link {

namespace influx {

    struct tag {
        std::string name;
        std::string field;
    };
    struct field {
        std::string name;
        std::variant<std::string, bool, std::int_fast64_t, double, std::size_t, std::uint8_t, std::uint16_t, std::uint32_t> value;
    };
}

/**
 * @brief The database class
 */
class database {
public:
    class entry {
    public:
        entry() = delete;

        auto operator<<(const influx::tag& tag) -> entry&;
        auto operator<<(const influx::field& field) -> entry&;

        [[nodiscard]] auto commit(std::int_fast64_t timestamp) -> bool;

    private:
        std::ostringstream m_tags {};
        std::ostringstream m_fields {};

        database& m_link;

        friend class database;

        entry(const std::string& measurement, database& link);
    };

    database(Config::Influx config);
    database();
    ~database();

    [[nodiscard]] auto measurement(const std::string& measurement) -> entry;

private:
    [[nodiscard]] auto send_string(const std::string& query) const -> bool;

    static constexpr short s_port { 8086 };

    std::mutex m_mutex;

    Config::Influx m_config { Config::influx };
};

}

#endif // DATABASELINK_H
