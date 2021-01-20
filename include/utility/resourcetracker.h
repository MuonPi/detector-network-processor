#ifndef RESOURCETRACKER_H
#define RESOURCETRACKER_H

#include <cstdint>

extern "C" {
#include <glibtop.h>
#include <glibtop/cpu.h>
#include <glibtop/mem.h>
}

namespace MuonPi {

class ResourceTracker {
public:
    [[nodiscard]] auto cpu_load() -> float;
    [[nodiscard]] auto memory_usage() -> float;

private:
    struct {
        std::uint64_t total_time_last {};
        std::uint64_t used_time_last {};
    } m_cpu {};
};
}

#endif // RESOURCETRACKER_H
