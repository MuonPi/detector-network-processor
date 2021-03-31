#ifndef TRIGGER_H
#define TRIGGER_H

#include <cinttypes>
#include <string>

namespace muonpi::Trigger {

struct Detector {
    struct Setting {
        enum Type {
            Invalid,
            Online,
            Offline,
            Reliable,
            Unreliable
        } type { Invalid };

        std::string username;
        std::string station;

        [[nodiscard]] auto to_string(char delimiter = 0) const -> std::string;

        [[nodiscard]] auto id() const -> std::size_t;

        [[nodiscard]] static auto from_string(const std::string& string) -> Setting;
    } setting;

    struct Action {
        enum Type {
            Activate,
            Deactivate
        } type;
        Setting setting;
    };
};

}

#endif // TRIGGER_H
