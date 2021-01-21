#include "utility/resourcetracker.h"

#include <algorithm>
#include <fstream>
#include <string>
#include <unistd.h>

namespace MuonPi {

auto ResourceTracker::get_data() -> Data
{
    std::ifstream total_stream("/proc/stat", std::ios_base::in);

    std::size_t cpu_total {};
    std::size_t system_user {};
    std::size_t system_nice {};
    std::size_t system_system {};

    std::string cpu;

    total_stream >> cpu >> system_user >> system_nice >> system_system;

    cpu_total += system_user + system_nice + system_system;

    for (std::size_t i { 0 }; i < 7; i++) {
        std::size_t v {};
        total_stream >> v;
        cpu_total += v;
    }

    total_stream.close();

    std::ifstream stat_stream("/proc/self/stat", std::ios_base::in);

    std::string pid, comm, state, ppid, pgrp, session, tty_nr;
    std::string tpgid, flags, minflt, cminflt, majflt, cmajflt;
    std::string utime, stime, cutime, cstime, priority, nice;
    std::string O, itrealvalue, starttime;

    std::size_t process_user;
    std::size_t process_system;

    std::size_t vsize;
    std::size_t rss;

    stat_stream >> pid >> comm >> state >> ppid >> pgrp >> session >> tty_nr
        >> tpgid >> flags >> minflt >> cminflt >> majflt >> cmajflt
        >> process_user >> process_system >> cutime >> cstime >> priority >> nice
        >> O >> itrealvalue >> starttime >> vsize >> rss;
    stat_stream.close();

    long page_size_b = sysconf(_SC_PAGE_SIZE);

    float total = cpu_total - m_cpu.total_time_last;
    float process = process_user + process_system - m_cpu.process_time_last;
    float system = system_user + system_system + system_nice - m_cpu.system_time_last;
    m_cpu.total_time_last = cpu_total;
    m_cpu.process_time_last = process_user + process_system;
    m_cpu.system_time_last = system_user + system_system + system_nice;

    Data data;
    data.memory_usage = rss * page_size_b;
    data.process_cpu_load = 0;
    data.system_cpu_load = 0;

    if (!m_first) {
        data.process_cpu_load = 100.0 * process / std::max(total, 1.0f);
        data.system_cpu_load = 100.0 * system / std::max(total, 1.0f);
    } else {
        m_first = false;
    }

    return data;
}

}
