#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <variant>

namespace muonpi {

class configuration {
public:
    struct definition {
        definition(std::string name, int* value);
        definition(std::string name, bool* value);
        definition(std::string name, double* value);
        definition(std::string name, std::string* value);
        definition();

        [[nodiscard]] auto name() const -> std::string;

        template <typename T>
        auto operator=(T value) -> bool;

        [[nodiscard]] auto write() -> std::string;

        [[nodiscard]] auto read(const std::string& in) -> bool;

        [[nodiscard]] operator bool() const;

    private:
        template <typename T>
        auto set(T value) -> bool;

        std::variant<bool*, int*, double*, std::string*> m_option;
        bool m_valid { false };
        std::string m_name {};
    };

    configuration(std::string filename, bool encrypted = false);

    void set_encrypted(bool encrypted);
    void set_filename(const std::string& filename);

    [[nodiscard]] auto operator[](const std::string& name) -> definition&;

    auto operator<<(definition argument) -> configuration&;

    [[nodiscard]] auto read() -> bool;

    [[nodiscard]] auto write() -> bool;

private:
    [[nodiscard]] auto read(std::istream& in) -> bool;
    [[nodiscard]] auto write(std::ostream& out) -> bool;
    [[nodiscard]] static auto decrypt(std::istream& file) -> std::string;
    static void encrypt(std::ostream& file, const std::string& content);

    std::string m_filename {};
    bool m_encrypted { false };

    std::map<std::string, definition> m_options {};
};

template <typename T>
auto configuration::definition::operator=(T value) -> bool
{
    return set<T>(value);
}

template <typename T>
auto configuration::definition::set(T value) -> bool
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
