#pragma once

#include <libassert/assert.hpp>

#include <cassert>
#include <string_view>

#include "fs/types.h"

namespace db
{

static constexpr const std::string_view EMBEDDED_STR_VIEW{"embedded"};
static constexpr const std::string_view STANDALONE_STR_VIEW{"standalone"};
static constexpr const std::string_view REPLICATED_STR_VIEW{"replicated"};

enum class db_mode_t : int8_t
{
    kEmbedded = 0,
    kStandalone = 1,
    kReplicated = 2,
};

[[nodiscard]] auto to_string(const db_mode_t mode) noexcept -> std::string_view;
[[nodiscard]] auto from_string(const std::string_view mode) noexcept -> db_mode_t;

struct db_config_t
{
    fs::path_t  DatabasePath{"."};
    std::string WalFilename{"wal"};
    std::string ManifestFilenamePrefix{"manifest_"};
    db_mode_t   mode{db_mode_t::kStandalone};
};

} // namespace db
