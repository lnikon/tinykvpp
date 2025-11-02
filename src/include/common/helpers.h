#include <algorithm>
#include <filesystem>
#include <iterator>
#include <random>
#include <array>

#include <fmt/core.h>

#include "common/uuid.h"

namespace common
{

auto uuid() -> std::string;

auto segment_name() -> std::string;

auto segment_path(const std::filesystem::path &datadir, const std::string &name)
    -> std::filesystem::path;

} // namespace common
