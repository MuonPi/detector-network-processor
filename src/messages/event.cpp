#include "messages/event.h"

namespace muonpi {

auto event_t::duration() const noexcept -> std::int_fast64_t
{
    return end - start;
}

auto event_t::n() const noexcept -> std::size_t
{
    return events.size();
}

void event_t::emplace(event_t event) noexcept
{
    if (event.n() > 1) {
        for (const auto& e : event.events) {
            emplace(e);
        }
        return;
    }

    if (event.start < start) {
        start = event.start;
    } else if (event.start > end) {
        end = event.start;
    }

    events.emplace_back(std::move(event));
}

}
