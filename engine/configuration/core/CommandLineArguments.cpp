#include "core/CommandLineArguments.hpp"

#include <mutex>
#include <string>
#include <vector>

using std::string_view_literals::operator""sv;

namespace
{
    std::recursive_mutex argument_lock;
    std::vector<std::string> arguments;
    constexpr auto empty_argument{ ""sv };
}

namespace core
{
    void CommandLineArguments::clear()
    {
        std::lock_guard const lock{ argument_lock };
        arguments.clear();
    }

    void CommandLineArguments::add(std::string_view const argument)
    {
        std::lock_guard const lock{ argument_lock };
        arguments.emplace_back(argument);
    }

    void CommandLineArguments::assign(std::vector<std::string> const& new_arguments)
    {
        std::lock_guard const lock{ argument_lock };
        arguments = new_arguments;
    }

    std::string_view CommandLineArguments::at(size_t const index)
    {
        std::lock_guard const lock{ argument_lock };
        if(index < arguments.size()) {
            return arguments[index];
        }
        return empty_argument;
    }

    size_t CommandLineArguments::size()
    {
        std::lock_guard const lock{ argument_lock };
        return arguments.size();
    }

    std::vector<std::string> CommandLineArguments::copy()
    {
        std::lock_guard const lock{ argument_lock };
        return arguments;
    }
}
