#include "Main.h"
#include "core/CommandLineArguments.hpp"
#include "ApplicationRestart.hpp"
#include "Platform/MessageBox.hpp"
#include "LConfig.h"
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_main.h>
#include <cstdlib>
#include <string>
#include <vector>
#include <exception>

#undef main

#ifdef _WIN32
#define LUASTG_ENTRY_POINT SDL_main
#else
#define LUASTG_ENTRY_POINT main
#endif

int
LUASTG_ENTRY_POINT(const int argc, char* argv[])
{
    SDL_SetMainReady();
    try {
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
    } catch(const std::exception& error) {
        Platform::MessageBox::Error(LUASTG_INFO, error.what());
        return EXIT_FAILURE;
    }
}
