#include "analysis/eventconstructor.h"
#include "analysis/criterion.h"
#include "utility/log.h"

namespace muonpi {

void event_constructor::set_timeout(std::chrono::system_clock::duration new_timeout)
{
    if (new_timeout <= timeout) {
        return;
    }
    timeout = new_timeout;
}

auto event_constructor::timed_out() const -> bool
{
    return (std::chrono::system_clock::now() - m_start) >= timeout;
}

}
