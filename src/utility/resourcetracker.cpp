#include "utility/resourcetracker.h"

#include <algorithm>
#include <fstream>
#include <string>
#include <unistd.h>

namespace MuonPi {

auto ResourceTracker::get_data() -> Data
{
    std::ifstream stat_stream("/proc/self/stat", std::ios_base::in);

    std::string pid, comm, state, ppid, pgrp, session, tty_nr;
    std::string tpgid, flags, minflt, cminflt, majflt, cmajflt;
    std::string utime, stime, cutime, cstime, priority, nice;
    std::string O, itrealvalue, starttime;

    std::size_t cpu_total;
    std::size_t cpu_user;
    std::size_t cpu_system;

    std::size_t vsize;
    std::size_t rss;

    stat_stream >> pid >> comm >> state >> ppid >> pgrp >> session >> tty_nr
        >> tpgid >> flags >> minflt >> cminflt >> majflt >> cmajflt
        >> cpu_user >> cpu_system >> cpu_total >> cstime >> priority >> nice
        >> O >> itrealvalue >> starttime >> vsize >> rss;
    stat_stream.close();

    long page_size_b = sysconf(_SC_PAGE_SIZE);

    float total = cpu_total - m_cpu.total_time_last;
    float used = cpu_user + cpu_system - m_cpu.used_time_last;
    m_cpu.total_time_last = cpu_total;
    m_cpu.used_time_last = cpu_user + cpu_system;

    Data data;
    data.memory_usage = rss * page_size_b;
    data.cpu_load = 0;

    if (!m_first) {
        data.cpu_load = 100.0 * used / std::max(total, 1.0f);
    } else {
        m_first = false;
    }

    return data;
}

}
