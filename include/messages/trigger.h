#ifndef TRIGGER_H
#define TRIGGER_H

#include <cinttypes>
#include <string>

namespace MuonPi {

struct DetectorTrigger {
    enum Type {
        Online,
        Offline,
        Reliable,
        Unreliable
    } type;

    std::size_t target;

    std::string username;
    std::string station;
};

}


#endif // TRIGGER_H
