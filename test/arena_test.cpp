#include "core/arena.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

#include "core/assert.hpp"

using frankie::core::arena;
using frankie::core::arena_block;
using frankie::core::kBlockAlignment;
using frankie::core::kDefaultBlockSize;
using frankie::core::detail::block_capacity_rounded;
using frankie::core::detail::fixed_aligned_alloc;

namespace {

bool is_aligned(const void *ptr, std::uint64_t alignment) {
  return (reinterpret_cast<std::uintptr_t>(ptr) & (alignment - 1)) == 0;
}

}  // namespace

// ============================================================================
// detail::block_capacity_rounded
// ============================================================================

TEST(ArenaBlockCapacityRoundedTest, ZeroCapacityRoundsToHeaderMultiple) {
  const std::uint64_t rounded = block_capacity_rounded(0);
  EXPECT_EQ(rounded % kBlockAlignment, 0u);
  EXPECT_GE(rounded, sizeof(arena_block));
}

TEST(ArenaBlockCapacityRoundedTest, SmallCapacityRoundsUpToAlignment) {
  const std::uint64_t rounded = block_capacity_rounded(1);
  EXPECT_EQ(rounded % kBlockAlignment, 0u);
  EXPECT_GE(rounded, sizeof(arena_block) + 1u);
}

TEST(ArenaBlockCapacityRoundedTest, ExactAlignmentBoundaryIsUnchanged) {
  const std::uint64_t capacity = (kBlockAlignment * 4) - sizeof(arena_block);
  const std::uint64_t rounded = block_capacity_rounded(capacity);
  EXPECT_EQ(rounded, kBlockAlignment * 4u);
}

TEST(ArenaBlockCapacityRoundedTest, LargeCapacityIsMultipleOfAlignment) {
  const std::uint64_t rounded = block_capacity_rounded(kDefaultBlockSize);
  EXPECT_EQ(rounded % kBlockAlignment, 0u);
  EXPECT_GE(rounded, sizeof(arena_block) + kDefaultBlockSize);
  EXPECT_LT(rounded, sizeof(arena_block) + kDefaultBlockSize + kBlockAlignment);
}

// ============================================================================
// detail::fixed_aligned_alloc
// ============================================================================

TEST(ArenaFixedAlignedAllocTest, ReturnsBlockAlignedPointer) {
  arena_block *block = fixed_aligned_alloc(256);
  ASSERT_NE(block, nullptr);
  EXPECT_TRUE(is_aligned(block, kBlockAlignment));
  std::free(block);
}

TEST(ArenaFixedAlignedAllocTest, DataPointerIsBlockAligned) {
  arena_block *block = fixed_aligned_alloc(256);
  ASSERT_NE(block, nullptr);
  EXPECT_TRUE(is_aligned(block->data(), kBlockAlignment));
  std::free(block);
}

// ============================================================================
// arena_block layout
// ============================================================================

TEST(ArenaBlockTest, IsCacheLineAligned) {
  EXPECT_EQ(alignof(arena_block), kBlockAlignment);
  EXPECT_EQ(sizeof(arena_block) % kBlockAlignment, 0u);
}

// ============================================================================
// arena construction / destruction
// ============================================================================

TEST(ArenaTest, DefaultConstructedHasNoAllocation) {
  arena a;
  EXPECT_EQ(a.bytes_allocated(), 0u);
}

TEST(ArenaTest, CreateReservesBlockAndReportsBytes) {
  const std::uint64_t cap = 1024;
  arena a = arena::create(cap);
  EXPECT_EQ(a.bytes_allocated(), block_capacity_rounded(cap));
  a.destroy();
}

TEST(ArenaTest, CreateZeroCapacityDoesNotCrash) {
  arena a = arena::create(0);
  EXPECT_EQ(a.bytes_allocated(), block_capacity_rounded(0));
  a.destroy();
}

TEST(ArenaTest, DestroyResetsState) {
  arena a = arena::create(1024);
  (void)a.allocate(128, 8);
  EXPECT_GT(a.bytes_allocated(), 0u);
  a.destroy();
  EXPECT_EQ(a.bytes_allocated(), 0u);
}

TEST(ArenaTest, DestroyIsIdempotent) {
  arena a = arena::create(1024);
  a.destroy();
  a.destroy();
  EXPECT_EQ(a.bytes_allocated(), 0u);
}

TEST(ArenaTest, DestructorOnDefaultConstructedIsSafe) {
  {
    arena a;
    EXPECT_EQ(a.bytes_allocated(), 0u);
  }
  SUCCEED();
}

// ============================================================================
// arena::allocate - basic
// ============================================================================

TEST(ArenaTest, AllocateOnDefaultArenaCreatesBlock) {
  arena a;
  void *p = a.allocate(64, 8);
  ASSERT_NE(p, nullptr);
  EXPECT_GT(a.bytes_allocated(), 0u);
}

TEST(ArenaTest, AllocateReturnsNonNull) {
  arena a = arena::create(1024);
  EXPECT_NE(a.allocate(16, 8), nullptr);
}

TEST(ArenaTest, AllocateIsUsableForWrites) {
  arena a = arena::create(1024);
  auto *buf = static_cast<char *>(a.allocate(128, 8));
  ASSERT_NE(buf, nullptr);
  std::memset(buf, 0xAB, 128);
  for (int i = 0; i < 128; ++i) {
    EXPECT_EQ(static_cast<unsigned char>(buf[i]), 0xABu);
  }
}

TEST(ArenaTest, SequentialAllocationsAreNonOverlapping) {
  arena a = arena::create(4096);
  auto *p1 = static_cast<char *>(a.allocate(64, 8));
  auto *p2 = static_cast<char *>(a.allocate(64, 8));
  auto *p3 = static_cast<char *>(a.allocate(64, 8));
  ASSERT_NE(p1, nullptr);
  ASSERT_NE(p2, nullptr);
  ASSERT_NE(p3, nullptr);
  EXPECT_GE(p2, p1 + 64);
  EXPECT_GE(p3, p2 + 64);
}

TEST(ArenaTest, SequentialAllocationsAreBumpPointer) {
  arena a = arena::create(4096);
  auto *p1 = static_cast<char *>(a.allocate(32, 8));
  auto *p2 = static_cast<char *>(a.allocate(32, 8));
  EXPECT_EQ(p2, p1 + 32);
}

// ============================================================================
// arena::allocate - alignment
// ============================================================================

TEST(ArenaTest, AllocateRespectsAlignment1) {
  arena a = arena::create(1024);
  void *p = a.allocate(3, 1);
  EXPECT_TRUE(is_aligned(p, 1));
}

TEST(ArenaTest, AllocateRespectsAlignment8) {
  arena a = arena::create(1024);
  (void)a.allocate(3, 1);
  void *p = a.allocate(8, 8);
  EXPECT_TRUE(is_aligned(p, 8));
}

TEST(ArenaTest, AllocateRespectsAlignment16) {
  arena a = arena::create(1024);
  (void)a.allocate(1, 1);
  void *p = a.allocate(16, 16);
  EXPECT_TRUE(is_aligned(p, 16));
}

TEST(ArenaTest, AllocateRespectsAlignment32) {
  arena a = arena::create(1024);
  (void)a.allocate(1, 1);
  void *p = a.allocate(32, 32);
  EXPECT_TRUE(is_aligned(p, 32));
}

TEST(ArenaTest, AllocateRespectsBlockAlignment) {
  arena a = arena::create(1024);
  (void)a.allocate(1, 1);
  void *p = a.allocate(64, kBlockAlignment);
  EXPECT_TRUE(is_aligned(p, kBlockAlignment));
}

TEST(ArenaTest, MixedAlignmentRequestsAllSatisfied) {
  arena a = arena::create(4096);
  const std::uint64_t alignments[] = {1, 2, 4, 8, 16, 32, 64};
  for (auto align : alignments) {
    void *p = a.allocate(align, align);
    EXPECT_TRUE(is_aligned(p, align)) << "alignment=" << align;
  }
}

// ============================================================================
// arena::allocate - block chaining
// ============================================================================

TEST(ArenaTest, AllocationExceedingBlockTriggersNewBlock) {
  const std::uint64_t cap = 256;
  arena a = arena::create(cap);
  const std::uint64_t before = a.bytes_allocated();
  (void)a.allocate(200, 8);
  EXPECT_EQ(a.bytes_allocated(), before);
  (void)a.allocate(200, 8);
  EXPECT_GT(a.bytes_allocated(), before);
}

TEST(ArenaTest, OversizedAllocationGrowsBlockSize) {
  const std::uint64_t cap = 128;
  arena a = arena::create(cap);
  const std::uint64_t before = a.bytes_allocated();
  const std::uint64_t oversized = 4096;
  (void)a.allocate(oversized, 8);
  EXPECT_GE(a.bytes_allocated(), before + block_capacity_rounded(oversized));
}

TEST(ArenaTest, ManySmallAllocationsStaySingleBlock) {
  arena a = arena::create(4096);
  const std::uint64_t before = a.bytes_allocated();
  for (int i = 0; i < 100; ++i) {
    (void)a.allocate(16, 8);
  }
  EXPECT_EQ(a.bytes_allocated(), before);
}

TEST(ArenaTest, BytesAllocatedIsMonotonicallyNonDecreasing) {
  arena a;
  std::uint64_t last = a.bytes_allocated();
  for (int i = 0; i < 50; ++i) {
    (void)a.allocate(1024, 8);
    const std::uint64_t now = a.bytes_allocated();
    EXPECT_GE(now, last);
    last = now;
  }
}

TEST(ArenaTest, AllocationFromNewBlockStartsFreshOffset) {
  arena a = arena::create(256);
  auto *p1 = static_cast<char *>(a.allocate(200, 8));
  auto *p2 = static_cast<char *>(a.allocate(200, 8));
  // p2 must be in a different block, so it is not contiguous with p1.
  EXPECT_LT(std::abs(p2 - p1), static_cast<std::ptrdiff_t>(0x1000000))
      << "pointers live in the same process, sanity only";
  // And it must start on a block-aligned boundary (data() of a new block).
  EXPECT_TRUE(is_aligned(p2, kBlockAlignment));
}

// ============================================================================
// arena move semantics
// ============================================================================

TEST(ArenaTest, MoveConstructorTransfersOwnership) {
  arena src = arena::create(1024);
  (void)src.allocate(128, 8);
  const std::uint64_t src_bytes = src.bytes_allocated();

  arena dst{std::move(src)};
  EXPECT_EQ(dst.bytes_allocated(), src_bytes);
  EXPECT_EQ(src.bytes_allocated(), 0u);  // NOLINT(bugprone-use-after-move)
}

TEST(ArenaTest, MoveAssignmentTransfersOwnership) {
  arena src = arena::create(1024);
  (void)src.allocate(128, 8);
  const std::uint64_t src_bytes = src.bytes_allocated();

  arena dst;
  dst = std::move(src);
  EXPECT_EQ(dst.bytes_allocated(), src_bytes);
  EXPECT_EQ(src.bytes_allocated(), 0u);  // NOLINT(bugprone-use-after-move)
}

TEST(ArenaTest, MoveAssignmentReleasesPreviousBlocks) {
  arena dst = arena::create(1024);
  (void)dst.allocate(512, 8);

  arena src = arena::create(2048);
  (void)src.allocate(256, 8);
  const std::uint64_t src_bytes = src.bytes_allocated();

  dst = std::move(src);
  EXPECT_EQ(dst.bytes_allocated(), src_bytes);
}

TEST(ArenaTest, MoveSelfAssignmentIsSafe) {
  arena a = arena::create(1024);
  (void)a.allocate(128, 8);
  const std::uint64_t before = a.bytes_allocated();

  arena *self = &a;
  a = std::move(*self);
  EXPECT_EQ(a.bytes_allocated(), before);
  (void)a.allocate(64, 8);  // still usable
}

TEST(ArenaTest, MovedFromArenaIsUsableAfterMove) {
  arena src = arena::create(1024);
  arena dst{std::move(src)};
  // NOLINTNEXTLINE(bugprone-use-after-move)
  void *p = src.allocate(64, 8);
  EXPECT_NE(p, nullptr);
  EXPECT_GT(src.bytes_allocated(), 0u);  // NOLINT(bugprone-use-after-move)
}

// ============================================================================
// arena::allocate - stress
// ============================================================================

TEST(ArenaTest, ManyAllocationsDoNotCorruptMemory) {
  arena a;
  std::vector<char *> ptrs;
  ptrs.reserve(1024);
  for (int i = 0; i < 1024; ++i) {
    auto *p = static_cast<char *>(a.allocate(32, 8));
    ASSERT_NE(p, nullptr);
    std::memset(p, i & 0xFF, 32);
    ptrs.push_back(p);
  }
  for (std::size_t i = 0; i < ptrs.size(); ++i) {
    const auto expected = static_cast<unsigned char>(i & 0xFF);
    for (int j = 0; j < 32; ++j) {
      ASSERT_EQ(static_cast<unsigned char>(ptrs[i][j]), expected) << "i=" << i << " j=" << j;
    }
  }
}

// ============================================================================
// Death tests - debug asserts on bad alignment
// ============================================================================

#if FR_ASSERT_LEVEL >= 3

TEST(ArenaDeathTest, ZeroAlignmentAborts) {
  EXPECT_DEATH(
      {
        frankie::assert_detail::set_failure_handler(nullptr);
        arena a = arena::create(1024);
        (void)a.allocate(8, 0);
      },
      "DEBUG_ASSERT FAILED");
}

TEST(ArenaDeathTest, NonPowerOfTwoAlignmentAborts) {
  EXPECT_DEATH(
      {
        frankie::assert_detail::set_failure_handler(nullptr);
        arena a = arena::create(1024);
        (void)a.allocate(8, 3);
      },
      "DEBUG_ASSERT FAILED");
}

TEST(ArenaDeathTest, AlignmentExceedingBlockAlignmentAborts) {
  EXPECT_DEATH(
      {
        frankie::assert_detail::set_failure_handler(nullptr);
        arena a = arena::create(1024);
        (void)a.allocate(8, kBlockAlignment * 2);
      },
      "DEBUG_ASSERT FAILED");
}

#endif
