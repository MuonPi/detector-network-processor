#ifndef CONFIGURATION_H
#define CONFIGURATION_H


#include <string>
#include <optional>
#include <variant>
#include <iostream>
#include <map>

namespace MuonPi {

struct Option {
    Option(std::string  name, int* value);
    Option(std::string  name, bool* value);
    Option(std::string  name, double* value);
    Option(std::string  name, std::string* value);
    Option();

    [[nodiscard]] auto name() const -> std::string;

    template <typename T>
    auto operator=(T value) -> bool;

    [[nodiscard]] auto write() -> std::string;

    [[nodiscard]] auto read(const std::string& in) -> bool;

    [[nodiscard]] operator bool() const;
private:
    template <typename T>
    auto set(T value) -> bool;

    std::variant<bool*,int*,double*,std::string*> m_option;
    bool m_valid { false };
    std::string m_name {};
};

class Configuration
{
public:
    Configuration(std::string  filename, bool encrypted = false);

    void set_encrypted(bool encrypted);
    void set_filename(const std::string& filename);

    [[nodiscard]] auto operator[](const std::string& name) -> Option&;

    auto operator<<(Option argument) -> Configuration&;

    [[nodiscard]] auto read() -> bool;

    [[nodiscard]] auto write() -> bool;

private:
    [[nodiscard]] auto read(std::istream& in) -> bool;
    [[nodiscard]] auto write(std::ostream& out) -> bool;
    [[nodiscard]] static auto decrypt(std::istream &file) -> std::string;
    static void encrypt(std::ostream &file, const std::string& content);


    std::string m_filename {};
    bool m_encrypted { false };

    std::map<std::string, Option> m_options {};
};

template <typename  T>
auto Option::operator=(T value) -> bool
{
    return set<T>(value);
}

template <typename  T>
auto Option::set(T value) -> bool
{
    if (!std::holds_alternative<T*>(m_option)) {
        return false;
    }
    T* opt { std::get<T*>(m_option) };
    if (opt == nullptr) {
        return false;
    }
    *opt = value;
    return true;
}



}
#endif // CONFIGURATION_H
