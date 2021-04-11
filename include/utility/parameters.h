#ifndef PARAMETERS_H
#define PARAMETERS_H

#include <string>
#include <vector>

namespace muonpi {
class parameters {
public:
    struct definition {
        const std::string abbreviation {};
        const std::string full {};
        const std::string description {};
        const bool value { false };
        const bool required { false };
    };

    struct state_t {
        bool set { false };
        std::string value {};

        operator bool() const
        {
            return set;
        }
    };

    parameters(std::string name, std::string description);

    [[nodiscard]] auto get(const std::string& name) const -> state_t;
    [[nodiscard]] auto operator[](const std::string& name) const -> state_t;

    void add(const definition& argument);
    auto operator<<(const definition& argument) -> parameters&;

    auto start(std::vector<std::string> arguments) -> bool;

    void print_help() const;

private:
    struct commandline {
        definition def;
        state_t state;
    };

    std::size_t m_required { 0 };

    std::string m_name {};
    std::string m_description {};

    std::vector<commandline> m_arguments;
};
}
#endif // PARAMETERS_H
