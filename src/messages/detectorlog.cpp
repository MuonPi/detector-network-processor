#include "messages/detectorlog.h"

#include <utility>

namespace muonpi {

void detector_log_t::emplace(item it)
{
    items.emplace(std::move(it));
}

auto detector_log_t::get() -> item
{
    item it { items.front() };
    items.pop();
    return it;
}

}
