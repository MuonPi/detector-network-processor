#include "application.h"

#include <algorithm>

auto main(int argc, const char* argv[]) -> int
{

    std::vector<std::string> args {};
    for (int i { 1 }; i < argc; i++) {
        args.emplace_back(argv[i]);
    }

    muonpi::application application {};

    if (!application.setup(args)) {
        return 1;
    }

    return application.run();
}
