#include "application.h"

auto main(int argc, const char* argv[]) -> int
{
    if (!muonpi::application::setup(argc, argv)) {
        return 0;
    }

    return muonpi::application::run();
}
