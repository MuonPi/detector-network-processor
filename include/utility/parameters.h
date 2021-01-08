#ifndef PARAMETERS_H
#define PARAMETERS_H

#include <string>
#include <vector>

namespace MuonPi {
class Parameters {
public:
    struct Definition {
        const std::string abbreviation {};
        const std::string full {};
        const std::string description {};
        const bool value { false };
        const bool required { false };
    };

    struct State {
        bool set { false };
        std::string value {};

        operator bool() const
        {
            return set;
        }
    };

    Parameters(std::string name, std::string description);

    [[nodiscard]] auto get(const std::string& name) const -> State;
    [[nodiscard]] auto operator[](const std::string& name) const -> State;

    void add(const Definition& argument);
    auto operator<<(const Definition& argument) -> Parameters&;

    auto start(int argc, char* argv[]) -> bool;

    void print_help() const;

private:
    struct Commandline {
        Definition def;
        State state;
    };

    int m_required { 0 };

    std::string m_name {};
    std::string m_description {};

    std::vector<Commandline> m_arguments;
};
}
#endif // PARAMETERS_H
