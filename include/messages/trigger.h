#ifndef TRIGGER_H
#define TRIGGER_H

#include "analysis/detectorstation.h"
#include "messages/detectorstatus.h"
#include "messages/userinfo.h"

#include <cinttypes>
#include <string>

namespace muonpi::trigger {

struct detector {
    std::size_t hash {};

    userinfo_t userinfo {};
    detector_status::status status { detector_status::invalid };
    detector_status::reason reason { detector_status::reason::miscellaneous };
};

}

#endif // TRIGGER_H
