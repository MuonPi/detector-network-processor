#ifndef TRIGGER_H
#define TRIGGER_H

#include "messages/userinfo.h"

#include <cinttypes>
#include <string>

namespace muonpi::trigger {

struct detector {
    struct setting_t {
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

        [[nodiscard]] static auto from_string(const std::string& string) -> setting_t;
    } setting;

    std::size_t hash {};

    struct action_t {
        enum Type {
            Activate,
            Deactivate
        } type;
        setting_t setting;
    };
};

}

#endif // TRIGGER_H
