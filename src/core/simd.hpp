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
__attribute__((target("avx2"))) int simd_compare_avx2(const char *a, const char *b, size_t len) noexcept;
#endif

#ifdef __SSE2__
__attribute__((target("sse2"))) int simd_compare_sse2(const char *a, const char *b, size_t len) noexcept;
#endif

#endif  // FRANKIE_SIMD_H
