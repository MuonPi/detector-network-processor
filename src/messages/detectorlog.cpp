#include "messages/detectorlog.h"

#include <utility>

namespace muonpi {

void detector_log_t::emplace(item item)
{
    items.emplace(std::move(item));
}

auto detector_log_t::get() -> item
{
    item item { items.front() };
    items.pop();
    return item;
}

}
