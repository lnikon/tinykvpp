#include <ctime>

#include "core/time.hpp"

namespace frankie::core {

/**
 * To justify the choice of CLOCK_REALTIME, consider the three clock families Linux provides:
 *
 * CLOCK_MONOTONIC / CLOCK_MONOTONIC_RAW — a good stopwatch for elapsed time, but not for
 * pinpointing an absolute moment. Epoch is undefined (typically time-since-boot) and resets
 * on reboot, so timestamps persisted to disk are meaningless after restart. Strictly monotonic
 * within a single boot session. Does not count time spent in suspend.
 *
 * CLOCK_REALTIME — survives reboot. Epoch is Unix time. Fragile due to NTP adjustments and
 * manual clock changes (can jump backward). Dangerous to use for ordering — that is what
 * sequence numbers are for — but a good fit for TTL expiry and human-readable diagnostics.
 *
 * CLOCK_BOOTTIME — same as CLOCK_MONOTONIC, but counts time spent in suspend. Same reboot
 * problem: values are meaningless across restarts.
 *
 * We use CLOCK_REALTIME for the timestamp field and treat it as advisory metadata only.
 * The sequence number is the sole authority for entry ordering.
 */
std::uint64_t wall_clock_ms() noexcept {
  timespec timespec;
  clock_gettime(CLOCK_REALTIME, &timespec);
  return static_cast<std::uint64_t>(timespec.tv_sec) * 1000 + static_cast<std::uint64_t>(timespec.tv_nsec) / 1'000'000;
}

}  // namespace frankie::core
