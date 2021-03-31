#include "messages/event.h"
#include "utility/log.h"

namespace muonpi {

event_t::event_t(std::size_t hash, data_t data) noexcept
    : m_hash { hash }
    , m_data { std::move(data) }
{
}

event_t::event_t(event_t event, bool /*foreign*/) noexcept
    : event_t { event.hash(), event.data() }
{
    m_data.end = m_data.start;

    m_events.push_back(std::move(event));
}

event_t::event_t() noexcept
    : m_valid { false }
{
}

event_t::~event_t() noexcept = default;

auto event_t::start() const noexcept -> std::int_fast64_t
{
    return m_data.start;
}

auto event_t::duration() const noexcept -> std::int_fast64_t
{
    return m_data.end - m_data.start;
}

auto event_t::end() const noexcept -> std::int_fast64_t
{
    return m_data.end;
}

auto event_t::hash() const noexcept -> std::size_t
{
    return m_hash;
}

auto event_t::n() const noexcept -> std::size_t
{
    return m_n;
}

void event_t::add_event(event_t event) noexcept
{
    if (event.n() > 1) {
        for (const auto& e : event.events()) {
            add_event(e);
        }
        return;
    }

    if (event.start() < start()) {
        m_data.start = event.start();
    } else if (event.start() > end()) {
        m_data.end = event.start();
    }

    m_events.push_back(std::move(event));
    m_n++;
}

auto event_t::events() const -> const std::vector<event_t>&
{
    return m_events;
}

auto event_t::valid() const -> bool
{
    return m_valid;
}
auto event_t::data() const -> data_t
{
    return m_data;
}

void event_t::set_data(const data_t& data)
{
    m_data = data;
}

void event_t::set_detector_info(location_t location, userinfo_t user)
{
    m_location = std::move(location);
    m_user_info = std::move(user);
}

auto event_t::location() const -> location_t
{
    return m_location;
}

auto event_t::user_info() const -> userinfo_t
{
    return m_user_info;
}

}
