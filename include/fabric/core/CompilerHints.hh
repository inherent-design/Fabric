#pragma once

/// Cross-platform compiler hint macros for Fabric Engine.
/// Abstracts GCC/Clang attributes and MSVC equivalents.

#if defined(_MSC_VER)
#define FABRIC_ALWAYS_INLINE __forceinline
#define FABRIC_NOINLINE __declspec(noinline)
#else
#define FABRIC_ALWAYS_INLINE [[gnu::always_inline]] inline
#define FABRIC_NOINLINE [[gnu::noinline]]
#endif
