#pragma once

// Fabric Profiler Abstraction
// When FABRIC_PROFILING_ENABLED is defined, these map to Tracy.
// Otherwise, they compile to nothing.

#ifdef FABRIC_PROFILING_ENABLED
#include <tracy/Tracy.hpp>
#include <tracy/TracyC.h>

// --- Zone Profiling ---
#define FABRIC_ZONE_SCOPED ZoneScoped
#define FABRIC_ZONE_SCOPED_N(name) ZoneScopedN(name)
#define FABRIC_ZONE_SCOPED_C(color) ZoneScopedC(color)
#define FABRIC_ZONE_SCOPED_NC(name, c) ZoneScopedNC(name, c)
#define FABRIC_ZONE_TEXT(txt, len) ZoneText(txt, len)
#define FABRIC_ZONE_NAME(txt, len) ZoneName(txt, len)
#define FABRIC_ZONE_VALUE(val) ZoneValue(val)
#define FABRIC_ZONE_COLOR(color) ZoneColor(color)

// With callstack capture (depth = number of frames to capture)
#define FABRIC_ZONE_SCOPED_S(depth) ZoneScopedS(depth)
#define FABRIC_ZONE_SCOPED_NS(name, depth) ZoneScopedNS(name, depth)

// --- Frame Marking ---
#define FABRIC_FRAME_MARK FrameMark
#define FABRIC_FRAME_MARK_NAMED(name) FrameMarkNamed(name)
#define FABRIC_FRAME_MARK_START(name) FrameMarkStart(name)
#define FABRIC_FRAME_MARK_END(name) FrameMarkEnd(name)

// --- Memory Profiling ---
#define FABRIC_ALLOC(ptr, size) TracyAlloc(ptr, size)
#define FABRIC_FREE(ptr) TracyFree(ptr)
#define FABRIC_ALLOC_N(ptr, size, name) TracyAllocN(ptr, size, name)
#define FABRIC_FREE_N(ptr, name) TracyFreeN(ptr, name)
#define FABRIC_ALLOC_S(ptr, size, depth) TracyAllocS(ptr, size, depth)
#define FABRIC_FREE_S(ptr, depth) TracyFreeS(ptr, depth)

// --- Lock Profiling ---
#define FABRIC_LOCKABLE(type, var) TracyLockable(type, var)
#define FABRIC_LOCKABLE_N(type, var, desc) TracyLockableN(type, var, desc)
#define FABRIC_SHARED_LOCKABLE(type, var) TracySharedLockable(type, var)
#define FABRIC_SHARED_LOCKABLE_N(type, var, d) TracySharedLockableN(type, var, d)
#define FABRIC_LOCKABLE_BASE(type) LockableBase(type)
#define FABRIC_SHARED_LOCKABLE_BASE(type) SharedLockableBase(type)
#define FABRIC_LOCK_MARK(var) LockMark(var)

// --- Thread Naming ---
#define FABRIC_SET_THREAD_NAME(name) tracy::SetThreadName(name)

// --- Messages / Logging ---
#define FABRIC_MESSAGE(txt, len) TracyMessage(txt, len)
#define FABRIC_MESSAGE_L(txt) TracyMessageL(txt)

// --- Plots ---
#define FABRIC_PLOT(name, val) TracyPlot(name, val)
#define FABRIC_PLOT_CONFIG(name, type, step, fill, color) TracyPlotConfig(name, type, step, fill, color)

#else
// All macros compile to nothing when profiling is disabled.
#define FABRIC_ZONE_SCOPED
#define FABRIC_ZONE_SCOPED_N(name)
#define FABRIC_ZONE_SCOPED_C(color)
#define FABRIC_ZONE_SCOPED_NC(name, c)
#define FABRIC_ZONE_TEXT(txt, len)
#define FABRIC_ZONE_NAME(txt, len)
#define FABRIC_ZONE_VALUE(val)
#define FABRIC_ZONE_COLOR(color)
#define FABRIC_ZONE_SCOPED_S(depth)
#define FABRIC_ZONE_SCOPED_NS(name, depth)

#define FABRIC_FRAME_MARK
#define FABRIC_FRAME_MARK_NAMED(name)
#define FABRIC_FRAME_MARK_START(name)
#define FABRIC_FRAME_MARK_END(name)

#define FABRIC_ALLOC(ptr, size)
#define FABRIC_FREE(ptr)
#define FABRIC_ALLOC_N(ptr, size, name)
#define FABRIC_FREE_N(ptr, name)
#define FABRIC_ALLOC_S(ptr, size, depth)
#define FABRIC_FREE_S(ptr, depth)

// LOCKABLE no-ops must still declare the variable
#define FABRIC_LOCKABLE(type, var) type var
#define FABRIC_LOCKABLE_N(type, var, desc) type var
#define FABRIC_SHARED_LOCKABLE(type, var) type var
#define FABRIC_SHARED_LOCKABLE_N(type, var, d) type var
#define FABRIC_LOCKABLE_BASE(type) type
#define FABRIC_SHARED_LOCKABLE_BASE(type) type
#define FABRIC_LOCK_MARK(var)

#define FABRIC_SET_THREAD_NAME(name)
#define FABRIC_MESSAGE(txt, len)
#define FABRIC_MESSAGE_L(txt)
#define FABRIC_PLOT(name, val)
#define FABRIC_PLOT_CONFIG(name, type, step, fill, color)
#endif
