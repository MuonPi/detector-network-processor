#include "messages/detectorinfo.h"

#include <utility>

namespace muonpi {

auto detector_type::id() -> std::uint8_t
{
    return 0;
}

}
