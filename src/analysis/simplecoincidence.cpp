#include "analysis/simplecoincidence.h"
#include "messages/event.h"

#include <cmath>

#include <chrono>

namespace muonpi {

simple_coincidence::~simple_coincidence() = default;

auto simple_coincidence::compare(const event_t::data_t& first, const event_t::data_t& second) const -> double
{
    return (std::abs(first.start - second.start) <= s_time) ? 1.0 : -1.0;
}
} // namespace muonpi
