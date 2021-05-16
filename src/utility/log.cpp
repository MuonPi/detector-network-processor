#include "utility/log.h"

namespace muonpi::log {

auto debug() -> logger<Level::Debug>
{
    return {};
}

auto info() -> logger<Level::Info>
{
    return {};
}

auto notice() -> logger<Level::Notice>
{
    return {};
}

auto warning() -> logger<Level::Warning>
{
    return {};
}

auto error() -> logger<Level::Error>
{
    return {};
}

auto critical(int exit_code) -> logger<Level::Critical>
{
    return {std::move(exit_code)};
}

auto alert(int exit_code) -> logger<Level::Alert>
{
    return {std::move(exit_code)};
}

auto emergency(int exit_code) -> logger<Level::Emergency>
{
    return {std::move(exit_code)};
}

} // namespace muonpi::log
