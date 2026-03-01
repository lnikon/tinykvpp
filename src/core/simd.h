//
// Created by nikon on 3/1/26.
//

#ifndef FRANKIE_SIMD_H
#define FRANKIE_SIMD_H

#include <cpuid.h>
#include <immintrin.h>

#ifdef __AVX2__
#define HAS_AVX2 1
#endif
#ifdef __SSE2__
#define HAS_SSE2 1
#endif

#ifdef __AVX2__
__attribute__((target("avx2"))) int simd_compare_avx2(const char *a, const char *b, size_t len) noexcept {
  const char *a_start = a;
  const char *end = a + len;

  // Scan for the first differing 32-byte chunk
  for (; a + 32 <= end; a += 32, b += 32) {
    __m256i va = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(a));
    __m256i vb = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(b));
    __m256i neq = _mm256_cmpeq_epi8(va, vb);
    int mask = _mm256_movemask_epi8(neq);

    if (mask != -1) {
      // Find first mismatch bit within this chunk
      int first_diff = __builtin_ctz(~mask);  // count trailing 1s (equal bytes)
      // Now do a single byte compare
      unsigned char ca = static_cast<unsigned char>(a[first_diff]);
      unsigned char cb = static_cast<unsigned char>(b[first_diff]);
      return (ca > cb) - (ca < cb);  // -1, 0, or 1
    }
  }

  // Scalar tail
  for (; a < end; ++a, ++b) {
    unsigned char ca = static_cast<unsigned char>(*a);
    unsigned char cb = static_cast<unsigned char>(*b);
    if (ca != cb) return (ca > cb) - (ca < cb);
  }

  return 0;
}
#endif

#ifdef __SSE2__

__attribute__((target("sse2"))) inline int simd_compare_sse2(const char *a, const char *b, size_t len) noexcept {
  const char *end = a + len;

  for (; a + 16 <= end; a += 16, b += 16) {
    const __m128i va = _mm_loadu_si128(reinterpret_cast<const __m128i *>(a));
    const __m128i vb = _mm_loadu_si128(reinterpret_cast<const __m128i *>(b));
    const __m128i eq = _mm_cmpeq_epi8(va, vb);
    const int mask = _mm_movemask_epi8(eq);

    if (mask != 0xFFFF) {
      const int first_diff = __builtin_ctz(~mask);
      unsigned char ca = static_cast<unsigned char>(a[first_diff]);
      unsigned char cb = static_cast<unsigned char>(b[first_diff]);
      return (ca > cb) - (ca < cb);
    }
  }

  for (; a < end; ++a, ++b) {
    const unsigned char ca = static_cast<unsigned char>(*a);
    const unsigned char cb = static_cast<unsigned char>(*b);
    if (ca != cb) return (ca > cb) - (ca < cb);
  }

  return 0;
}

#endif

#endif  // FRANKIE_SIMD_H
