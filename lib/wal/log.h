#pragma once

#include "concepts.h"
#include "wal/in_memory_log_storage.h"
#include "wal/persistent_log_storage.h"

// It is possible to parametrize TLogStorageConcept with the Entity
// type e.g. TLogStorageConcept<std::string>. See the definitions of TLogStorageConcept.
template <TLogStorageConcept TStorage> class log_t
{
  public:
    explicit log_t(TStorage &&storage)
        : m_storage(std::move(storage))
    {
    }

    void append(std::string entry) noexcept
    {
        m_storage.append(std::move(entry));
    }

    [[nodiscard]] auto read(std::size_t index) const -> std::optional<std::string>
    {
        return m_storage.read(index);
    }

    [[nodiscard]] auto reset() -> bool
    {
        return m_storage.reset();
    }

  private:
    TStorage m_storage;
};
