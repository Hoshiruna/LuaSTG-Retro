#pragma once

#include <sdkddkver.h>

#include <vector>
#include <string>
#include <string_view>
#include <stdexcept>
#include <unordered_map>
#include <tuple>
#include <fstream>
#include <filesystem>

#include "spdlog/spdlog.h"
#include "nlohmann/json.hpp"

#define NOMINMAX
#include <Windows.h>
#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>

#include "utf8.hpp"
