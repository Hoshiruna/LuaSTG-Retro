#include "Main.h"
#include "core/CommandLineArguments.hpp"
#include "ApplicationRestart.hpp"
#include <SDL3/SDL_main.h>
#include <cstdlib>
#include <string>
#include <vector>

int main(const int argc, char* argv[])
{
    std::vector<std::string> arguments;
    arguments.reserve(static_cast<size_t>(argc));
    for(int i = 0; i < argc; ++i) {
        arguments.emplace_back(argv[i]);
    }
    core::CommandLineArguments::assign(arguments);

    const int code = luastg::main();
    if(luastg::ApplicationRestart::hasRestart()) {
        luastg::ApplicationRestart::start();
        return EXIT_SUCCESS;
    }
    return code;
}
