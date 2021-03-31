#include "utility/parameters.h"
#include "defaults.h"

#include <iostream>
#include <sstream>
#include <utility>

namespace muonpi {

Parameters::Parameters(std::string name, std::string description)
    : m_name { std::move(name) }
    , m_description { std::move(description) }
{
    add({ "h", "help", "Print this help" });
}

auto Parameters::get(const std::string& name) const -> State
{
    for (const auto& cmd : m_arguments) {
        if (name.compare(cmd.def.abbreviation) * name.compare(cmd.def.full) == 0) {
            return cmd.state;
        }
    }
    return {};
}

auto Parameters::operator[](const std::string& name) const -> State
{
    return get(name);
}

void Parameters::add(const Definition& argument)
{
    if (argument.required) {
        m_required++;
    }
    m_arguments.push_back({ argument, {} });
}

auto Parameters::operator<<(const Definition& argument) -> Parameters&
{
    add(argument);
    return *this;
}

auto Parameters::start(int argc, char* argv[]) -> bool
{
    if (m_required > 0) {
        if (argc < (1 + m_required)) {
            print_help();
            return false;
        }
    }
    if (argc <= 1) {
        return true;
    }
    int required { 0 };
    for (int j { 1 }; j < argc; j++) {
        std::string arg { argv[j] };
        bool found { false };
        for (auto& cmd : m_arguments) {
            if (arg.compare("-" + cmd.def.abbreviation) * arg.compare("--" + cmd.def.full) == 0) {
                if (cmd.def.required) {
                    required++;
                }
                if (cmd.def.value) {
                    if ((++j >= argc) || (argv[j][0] == '-')) {
                        std::cout << "expected name after " << arg << "\n";
                        print_help();
                        return false;
                    }
                    cmd.state.value = argv[j];
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

void Parameters::print_help() const
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
