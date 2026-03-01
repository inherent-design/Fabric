#include "fabric/core/BgfxCallback.hh"
#include "fabric/core/Log.hh"

#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

namespace fabric {

void BgfxCallback::fatal(const char* _filePath, uint16_t _line, bgfx::Fatal::Enum _code, const char* _str) {
    FABRIC_LOG_BGFX_CRITICAL("bgfx fatal [{}:{}] code={}: {}", _filePath, _line, static_cast<int>(_code), _str);
    std::abort();
}

void BgfxCallback::traceVargs(const char* _filePath, uint16_t _line, const char* _format, va_list _argList) {
    std::array<char, 2048> buf{};
    std::vsnprintf(buf.data(), buf.size(), _format, _argList);

    // Strip trailing newline that bgfx appends to trace messages
    auto len = std::strlen(buf.data());
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
        buf[--len] = '\0';
    }

    FABRIC_LOG_BGFX_DEBUG("[{}:{}] {}", _filePath, _line, buf.data());
}

void BgfxCallback::profilerBegin(const char* /*_name*/, uint32_t /*_abgr*/, const char* /*_filePath*/,
                                 uint16_t /*_line*/) {
    // No-op: Tracy integration handled separately via FABRIC_ZONE_SCOPED
}

void BgfxCallback::profilerBeginLiteral(const char* /*_name*/, uint32_t /*_abgr*/, const char* /*_filePath*/,
                                        uint16_t /*_line*/) {
    // No-op
}

void BgfxCallback::profilerEnd() {
    // No-op
}

uint32_t BgfxCallback::cacheReadSize(uint64_t /*_id*/) {
    return 0;
}

bool BgfxCallback::cacheRead(uint64_t /*_id*/, void* /*_data*/, uint32_t /*_size*/) {
    return false;
}

void BgfxCallback::cacheWrite(uint64_t /*_id*/, const void* /*_data*/, uint32_t /*_size*/) {
    // No shader cache for now
}

void BgfxCallback::screenShot(const char* /*_filePath*/, uint32_t /*_width*/, uint32_t /*_height*/, uint32_t /*_pitch*/,
                              bgfx::TextureFormat::Enum /*_format*/, const void* /*_data*/, uint32_t /*_size*/,
                              bool /*_yflip*/) {
    // No-op: screenshot capture not implemented
}

void BgfxCallback::captureBegin(uint32_t /*_width*/, uint32_t /*_height*/, uint32_t /*_pitch*/,
                                bgfx::TextureFormat::Enum /*_format*/, bool /*_yflip*/) {
    // No-op: video capture not implemented
}

void BgfxCallback::captureEnd() {
    // No-op
}

void BgfxCallback::captureFrame(const void* /*_data*/, uint32_t /*_size*/) {
    // No-op
}

} // namespace fabric
