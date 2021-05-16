#ifndef LOG_H
#define LOG_H

#include "defaults.h"

#include "utility/configuration.h"

#include <iostream>
#include <sstream>
#include <string>

namespace muonpi::log {

enum Level : int {
    Info,
    Emergency,
    Alert,
    Critical,
    Error,
    Warning,
    Notice,
    Debug
};

template <Level L>
class logger {
public:
    template <typename T>
    auto operator<<(T content) -> logger<L>&
    {
        m_stream << content;
        return *this;
    }

    ~logger()
    {
        if (L < (Level::Error + config::singleton()->meta.verbosity)) {
            std::clog << to_string() << m_stream.str() + "\n"
                      << std::flush;
        }
    }

private:
    std::ostringstream m_stream {};

    [[nodiscard]] auto to_string() -> std::string
    {
        switch (L) {
        case Level::Debug:
            return "Debug: ";
        case Level::Info:
            return "";
        case Level::Notice:
            return "Notice: ";
        case Level::Warning:
            return "Warning: ";
        case Level::Error:
            return "Error: ";
        case Level::Critical:
            return "Critical: ";
        case Level::Alert:
            return "Alert: ";
        case Level::Emergency:
            return "Emergency: ";
        }
        return {};
    }
};

[[nodiscard]] auto debug() -> logger<Level::Debug>;
[[nodiscard]] auto info() -> logger<Level::Info>;
[[nodiscard]] auto notice() -> logger<Level::Notice>;
[[nodiscard]] auto warning() -> logger<Level::Warning>;
[[nodiscard]] auto error() -> logger<Level::Error>;
[[nodiscard]] auto critical() -> logger<Level::Critical>;
[[nodiscard]] auto alert() -> logger<Level::Alert>;
[[nodiscard]] auto emergency() -> logger<Level::Emergency>;

}

#endif // LOG_H
