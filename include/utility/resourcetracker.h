#ifndef RESOURCETRACKER_H
#define RESOURCETRACKER_H

#include <cstdint>

namespace MuonPi {

class ResourceTracker {
public:
    struct Data {
        float cpu_load {};
        float memory_usage {};
    };

    [[nodiscard]] auto get_data() -> Data;

private:
    struct {
        std::uint64_t total_time_last {};
        std::uint64_t used_time_last {};
    } m_cpu {};

    bool m_first { true };
};
}

#endif // RESOURCETRACKER_H
