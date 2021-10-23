#include "analysis/eventconstructor.h"
#include "analysis/criterion.h"
#include <muonpi/log.h>

namespace muonpi {

void event_constructor::set_timeout(std::chrono::system_clock::duration new_timeout)
{
    if (new_timeout <= timeout) {
        return;
    }
    timeout = new_timeout;
}

auto event_constructor::timed_out(std::chrono::system_clock::time_point now) const -> bool
{
    return (now - m_start) >= timeout;
}

} // namespace muonpi
