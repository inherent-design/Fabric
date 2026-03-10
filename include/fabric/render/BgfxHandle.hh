#pragma once

#include <bgfx/bgfx.h>

namespace fabric {

// RAII wrapper for a single bgfx handle. Calls bgfx::destroy() in destructor.
// Move-only; deleted copy. Default-constructs to invalid state.
//
// Usage:
//   BgfxHandle<bgfx::ProgramHandle> program(bgfx::createProgram(...));
//   bgfx::submit(viewId, program.get());
//   // destroyed automatically when program goes out of scope
template <typename T> class BgfxHandle {
  public:
    BgfxHandle() noexcept { handle_.idx = bgfx::kInvalidHandle; }

    explicit BgfxHandle(T handle) noexcept : handle_(handle) {}

    ~BgfxHandle() { destroy(); }

    BgfxHandle(BgfxHandle&& other) noexcept : handle_(other.handle_) { other.handle_.idx = bgfx::kInvalidHandle; }

    BgfxHandle& operator=(BgfxHandle&& other) noexcept {
        if (this != &other) {
            destroy();
            handle_ = other.handle_;
            other.handle_.idx = bgfx::kInvalidHandle;
        }
        return *this;
    }

    BgfxHandle(const BgfxHandle&) = delete;
    BgfxHandle& operator=(const BgfxHandle&) = delete;

    T get() const noexcept { return handle_; }

    bool isValid() const noexcept { return bgfx::isValid(handle_); }

    // Destroy current handle and store a new one.
    void reset(T newHandle) {
        destroy();
        handle_ = newHandle;
    }

    // Destroy current handle and set to invalid.
    void reset() { destroy(); }

    // Return raw handle and relinquish ownership. Caller is responsible for destruction.
    T release() noexcept {
        T tmp = handle_;
        handle_.idx = bgfx::kInvalidHandle;
        return tmp;
    }

    bool operator==(const BgfxHandle& other) const noexcept { return handle_.idx == other.handle_.idx; }

    bool operator!=(const BgfxHandle& other) const noexcept { return handle_.idx != other.handle_.idx; }

  private:
    void destroy() {
        if (bgfx::isValid(handle_)) {
            bgfx::destroy(handle_);
            handle_.idx = bgfx::kInvalidHandle;
        }
    }

    T handle_;
};

} // namespace fabric
