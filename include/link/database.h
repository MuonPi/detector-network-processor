#ifndef DATABASELINK_H
#define DATABASELINK_H

#include "defaults.h"

#include <mutex>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

namespace muonpi::link {

namespace Influx {

    struct Tag {
        std::string name;
        std::string field;
    };
    struct Field {
        std::string name;
        std::variant<std::string, bool, std::int_fast64_t, double, std::size_t, std::uint8_t, std::uint16_t, std::uint32_t> value;
    };
}

/**
 * @brief The database class
 */
class database {
public:
    class Entry {
    public:
        Entry() = delete;

        auto operator<<(const Influx::Tag& tag) -> Entry&;
        auto operator<<(const Influx::Field& field) -> Entry&;

        [[nodiscard]] auto commit(std::int_fast64_t timestamp) -> bool;

    private:
        std::ostringstream m_tags {};
        std::ostringstream m_fields {};

        database& m_link;

        friend class database;

        Entry(const std::string& measurement, database& link);
    };

    database(Config::Influx config);
    database();
    ~database();

    [[nodiscard]] auto measurement(const std::string& measurement) -> Entry;

private:
    [[nodiscard]] auto send_string(const std::string& query) -> bool;

    static constexpr short s_port { 8086 };

    std::mutex m_mutex;

    Config::Influx m_config { Config::influx };
};

}

#endif // DATABASELINK_H
