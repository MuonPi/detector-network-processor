#include "utility/configuration.h"
#include "utility/log.h"
#include "utility/utility.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

#include <crypto++/base64.h>
#include <crypto++/files.h>
#include <crypto++/filters.h>
#include <crypto++/hex.h>
#include <crypto++/modes.h>
#include <crypto++/osrng.h>
#include <crypto++/sha.h>
#include <cryptopp/aes.h>

namespace MuonPi {

Option::Option(std::string name, int* value)
    : m_option { value }
    , m_valid { true }
    , m_name { std::move(name) }
{
}

Option::Option(std::string name, bool* value)
    : m_option { value }
    , m_valid { true }
    , m_name { std::move(name) }
{
}

Option::Option(std::string name, double* value)
    : m_option { value }
    , m_valid { true }
    , m_name { std::move(name) }
{
}

Option::Option(std::string name, std::string* value)
    : m_option { value }
    , m_valid { true }
    , m_name { std::move(name) }
{
}

Option::Option()

    = default;

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

auto Option::name() const -> std::string
{
    return m_name;
}

auto Option::write() -> std::string
{
    std::ostringstream out {};
    std::visit(overloaded {
                   [&](const bool* value) { out << m_name << "=b:" << (*value); },
                   [&](const int* value) { out << m_name << "=i:" << (*value); },
                   [&](const double* value) { out << m_name << "=d:" << (*value); },
                   [&](std::string* value) { out << m_name << "=s:" << (*value); },
               },
        m_option);
    return out.str();
}

auto Option::read(const std::string& in) -> bool
{
    if (in[1] != ':') {
        return false;
    }
    char type { in[0] };
    try {
        switch (type) {
        case 'b':
            return set<bool>(in.substr(2) == "true");
        case 'i':
            return set<int>(std::stoi(in.substr(2), nullptr));
        case 'd':
            return set<double>(std::stod(in.substr(2), nullptr));
        case 's':
            return set<std::string>(in.substr(2));
        default:
            return false;
        }
    } catch (...) {
        return false;
    }
}

Option::operator bool() const
{
    return m_valid;
}

Configuration::Configuration(std::string filename, bool encrypted)
    : m_filename { std::move(filename) }
    , m_encrypted { encrypted }
{
}
void Configuration::set_encrypted(bool encrypted)
{
    m_encrypted = encrypted;
}

void Configuration::set_filename(const std::string& filename)
{
    m_filename = filename;
}

auto Configuration::operator[](const std::string& name) -> Option&
{
    if (m_options.find(name) == m_options.end()) {
        throw -1;
    }
    return m_options[name];
}

auto Configuration::operator<<(Option argument) -> Configuration&
{
    m_options[argument.name()] = std::move(argument);
    return *this;
}

auto Configuration::read(std::istream& in) -> bool
{
    std::size_t n { 0 };
    std::string line {};
    while (std::getline(in, line)) {
        n++;
        auto equal_pos { line.find_first_of('=') };

        if (equal_pos == std::string::npos) {
            continue;
        }

        auto first_char { line.find_first_not_of(" \t") };
        if (first_char == std::string::npos) {
            continue;
        }
        line = line.substr(first_char);
        auto comment_pos { line.find_first_of('#') };

        if (comment_pos != std::string::npos) {
            if (comment_pos < equal_pos) {
                continue;
            }
            line = line.substr(0, comment_pos);
        }
        line = line.substr(0, line.find_last_not_of(" \t\n") + 1);

        auto colon_pos { line.substr(equal_pos + 1).find(':') };
        if ((equal_pos == std::string::npos) || (colon_pos == std::string::npos)) {
            continue;
        }

        std::string name { line.substr(0, equal_pos) };
        name = name.substr(0, name.find_last_not_of(" \t") + 1);
        if (m_options.find(name) == m_options.end()) {
            continue;
        }

        std::string value { line.substr(equal_pos + 1) };
        value = value.substr(value.find_first_not_of(" \t"));

        if (!m_options[name].read(value)) {
            Log::warning() << "Could not read config '" + m_filename + "' line " + std::to_string(n) + ": " + line;
            return false;
        }

        std::string output { "Loaded configuration: " + name };
        if (!m_encrypted) {
            output += "=" + value;
        }
        Log::debug() << output;
    }
    return true;
}

auto Configuration::read() -> bool
{
    if (!std::filesystem::exists(m_filename)) {
        Log::error() << "Configuration file does not exist: " + m_filename;
        return false;
    }
    bool result { false };
    if (m_encrypted) {
        std::ifstream file { m_filename };
        std::string decrypted { decrypt(file) };
        file.close();
        std::istringstream in_str { decrypted };
        result = read(in_str);
    } else {
        std::ifstream in { m_filename };
        result = read(in);
        in.close();
    }
    return result;
}

auto Configuration::write(std::ostream& out) -> bool
{
    for (auto& [name, option] : m_options) {
        out << option.write() << '\n';
    }
    return true;
}

auto Configuration::write() -> bool
{
    bool result { false };
    if (m_encrypted) {
        std::ostringstream plain {};
        result = write(plain);
        std::string string { plain.str() };
        std::ofstream file { m_filename };
        encrypt(file, string);
        file.close();

    } else {
        std::ofstream out { m_filename };
        result = write(out);
        out.close();
    }
    return result;
}

auto Configuration::decrypt(std::istream& file) -> std::string
{
    using namespace CryptoPP;

    std::ostringstream mac_stream {};
    mac_stream << std::hex << std::setfill('0') << std::setw(16) << GUID::get_mac();
    std::string mac { mac_stream.str() };

    SecByteBlock aes_key(reinterpret_cast<const byte*>(mac.data()), static_cast<int>(mac.size()));
    SecByteBlock iv(reinterpret_cast<const byte*>(mac.data()), static_cast<int>(mac.size()));

    CFB_Mode<AES>::Decryption dec { aes_key, mac.size(), iv, static_cast<int>(mac.size()) };

    std::string decrypted {};

    FileSource give_me_a_name(file, true, new Base64Decoder { new StreamTransformationFilter(dec, new StringSink(decrypted)) });
    return decrypted;
}

void Configuration::encrypt(std::ostream& file, const std::string& content)
{
    using namespace CryptoPP;
    std::ostringstream mac_stream {};
    mac_stream << std::hex << std::setfill('0') << std::setw(16) << GUID::get_mac();
    std::string mac { mac_stream.str() };

    SecByteBlock aes_key(reinterpret_cast<const byte*>(mac.data()), static_cast<int>(mac.size()));
    SecByteBlock iv(reinterpret_cast<const byte*>(mac.data()), static_cast<int>(mac.size()));

    std::string encrypted {};
    CFB_Mode<AES>::Encryption enc { aes_key, mac.size(), iv, static_cast<int>(mac.size()) };

    StringSource(content, true,
        new StreamTransformationFilter(enc, new Base64Encoder { new FileSink(file) }));
}
}
