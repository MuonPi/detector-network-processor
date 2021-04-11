#ifndef DETECTORSTATUS_H
#define DETECTORSTATUS_H

namespace muonpi {

namespace detector_status {
    enum status {
        invalid,
        deleted,
        created,
        unreliable,
        reliable
    };

    enum class reason {
        miscellaneous,
        time_accuracy,
        time_accuracy_extreme,
        location_precision,
        rate_unstable,
        missed_log_interval
    };
}
}

#endif // DETECTORSTATUS_H
