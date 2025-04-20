#include "db_config.h"
#include <string_view>

auto db::to_string(const db_mode_t mode) noexcept -> std::string_view
{
    switch (mode)
    {
    case db_mode_t::kEmbedded:
        return EMBEDDED_STR_VIEW;
    case db_mode_t::kStandalone:
        return STANDALONE_STR_VIEW;
    case db_mode_t::kReplicated:
        return REPLICATED_STR_VIEW;
    default:
        ASSERT(false, "provided database mode is not covered by the switch");
    }
}

auto db::from_string(const std::string_view mode) noexcept -> db_mode_t
{
    if (mode == db::EMBEDDED_STR_VIEW)
    {
        return db_mode_t::kEmbedded;
    }

    if (mode == db::STANDALONE_STR_VIEW)
    {
        return db_mode_t::kStandalone;
    }

    if (mode == db::REPLICATED_STR_VIEW)
    {
        return db_mode_t::kReplicated;
    }

    PANIC("provided database mode string is not supported", mode);
}