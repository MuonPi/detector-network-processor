#include "application.h"

auto main(int argc, const char* argv[]) -> int
{
    std::vector<std::string> arguments {};
    for (int i { 0 }; i < argc; i++) {
        arguments.emplace_back(argv[i]);
    }

    muonpi::application application {};

    if (!application.setup(arguments)) {
        return 1;
    }

    return application.run();
}
