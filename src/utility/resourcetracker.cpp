#include "utility/resourcetracker.h"

#include <algorithm>

namespace MuonPi {

auto ResourceTracker::memory_usage() -> float
{
    glibtop_init();
    glibtop_mem memory;
    glibtop_get_mem(&memory);

    return static_cast<float>(memory.used - memory.cached - memory.buffer);
}

auto ResourceTracker::cpu_load() -> float
{

    glibtop_cpu cpu;

    glibtop_get_cpu(&cpu);

    if (m_cpu.total_time_last == 0) {
        m_cpu.total_time_last = cpu.total;
        m_cpu.used_time_last = cpu.user + cpu.nice + cpu.sys;
        return 0.0;
    }

    float total = cpu.total - m_cpu.total_time_last;
    float used = cpu.user + cpu.sys - m_cpu.used_time_last;
    m_cpu.total_time_last = cpu.total;
    m_cpu.used_time_last = cpu.user + cpu.sys;

    float load = 100.0 * used / std::max(total, 1.0f);
    load = std::min(load, 100.0f);

    return load;
}

}
