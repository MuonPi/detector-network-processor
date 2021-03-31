#include "utility/parameters.h"
#include "defaults.h"

#include <iostream>
#include <sstream>
#include <utility>

namespace muonpi {

parameters::parameters(std::string name, std::string description)
    : m_name { std::move(name) }
    , m_description { std::move(description) }
{
    add({ "h", "help", "Print this help" });
}

auto parameters::get(const std::string& name) const -> state_t
{
    for (const auto& cmd : m_arguments) {
        if (name.compare(cmd.def.abbreviation) * name.compare(cmd.def.full) == 0) {
            return cmd.state;
        }
    }
    return {};
}

auto parameters::operator[](const std::string& name) const -> state_t
{
    return get(name);
}

void parameters::add(const definition& argument)
{
    if (argument.required) {
        m_required++;
    }
    m_arguments.push_back({ argument, {} });
}

auto parameters::operator<<(const definition& argument) -> parameters&
{
    add(argument);
    return *this;
}

auto parameters::start(std::vector<std::string> arguments) -> bool
{
    if (m_required > 0) {
        if (arguments.size() < (1 + m_required)) {
            print_help();
            return false;
        }
    }
    if (arguments.size() <= 1) {
        return true;
    }
    std::size_t required { 0 };
    for (std::size_t j { 1 }; j < arguments.size(); j++) {
        std::string arg { arguments[j] };
        bool found { false };
        for (auto& cmd : m_arguments) {

            if (arg.compare("-" + cmd.def.abbreviation) * arg.compare("--" + cmd.def.full) == 0) {
                if (cmd.def.required) {
                    required++;
                }
                if (cmd.def.value) {
                    if ((++j >= arguments.size()) || (arguments[j][0] == '-')) {
                        std::cout << "expected name after " << arg << "\n";
                        print_help();
                        return false;
                    }
                    cmd.state.value = arguments[j];
                }
                cmd.state.set = true;
                found = true;
                break;
            }
        }
        if (!found) {
            std::cout << "Invalid argument " << arg << '\n';
        }
    }
    if (required < m_required) {
        std::cout << "Not all required arguments were provided.\n\n";
        print_help();
        return false;
    }
    if (get("h")) {
        print_help();
        return false;
    }
    return true;
}

void parameters::print_help() const
{
    std::ostringstream out {};
    out << m_name << " v" << Version::string() << "\n"
        << m_description << "\n\nPossible arguments:\n";
    for (const auto& cmd : m_arguments) {
        out << "\t-" << cmd.def.abbreviation << "\t--" << cmd.def.full;
        if (cmd.def.value) {
            out << " (value)";
        }
        if (cmd.def.required) {
            out << " (required)";
        }
        out << "\n\t\t\t" << cmd.def.description << '\n';
    }
    std::cout << out.str() << std::flush;
}

}
