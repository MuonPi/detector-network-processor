#ifndef MUONDETECTOR_VERSION_H
#define MUONDETECTOR_VERSION_H

#include <chrono>

#cmakedefine CLUSTER_RUN_SERVER

namespace MuonPi::CMake::Version {
constexpr int major { @PROJECT_VERSION_MAJOR@ };
constexpr int minor { @PROJECT_VERSION_MINOR@ };
constexpr int patch { @PROJECT_VERSION_PATCH@ };
}

namespace MuonPi::Config {
namespace Interval {
constexpr std::chrono::steady_clock::duration clusterlog_interval { std::chrono::seconds{60} };
constexpr std::chrono::steady_clock::duration detectorsummary_interval { std::chrono::seconds{120} };
}
}

#endif // MUONDETECTOR_VERSION_H
