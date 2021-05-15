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

auto critical() -> logger<Level::Critical>
{
    return {};
}

auto alert() -> logger<Level::Alert>
{
    return {};
}

auto emergency() -> logger<Level::Emergency>
{
    return {};
}

}
