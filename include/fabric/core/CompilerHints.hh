#pragma once

/// Cross-platform compiler hint macros for Fabric Engine.
/// Abstracts GCC/Clang attributes and MSVC equivalents.

#if defined(_MSC_VER)
#define FABRIC_ALWAYS_INLINE __forceinline
#else
#define FABRIC_ALWAYS_INLINE [[gnu::always_inline]] inline
#endif
