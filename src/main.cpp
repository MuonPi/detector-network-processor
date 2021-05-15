#include "application.h"

#include <algorithm>

auto main(int argc, const char* argv[]) -> int
{
    muonpi::application application {};

    if (!application.setup(argc, argv)) {
        return 1;
    }

    return application.run();
}
