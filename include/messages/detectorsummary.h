#ifndef DETECTORSUMMARY_H
#define DETECTORSUMMARY_H

#include "userinfo.h"

#include <chrono>

namespace muonpi {

struct detector_summary_t {
    std::size_t hash { 0 };
    userinfo_t userinfo {};
    double deadtime { 0.0 };
    bool active { false };
    double mean_eventrate { 0.0 };
    double stddev_eventrate { 0.0 };
    double mean_pulselength { 0.0 };
    std::int64_t ublox_counter_progress { 0 };
    std::uint64_t incoming { 0UL };
    std::uint8_t change { 0 };
    double mean_time_acc { 0.0 };
};

}

#endif // DETECTORSUMMARY_H
