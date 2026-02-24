#pragma once

#include "fabric/utils/ErrorHandling.hh"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fabric::codec {

// Binary reader over a contiguous byte span. Tracks a cursor and
// throws FabricException on any out-of-bounds read.
class ByteReader {
  public:
    ByteReader(const uint8_t* data, size_t size) : buf_(data), size_(size), pos_(0) {}

    explicit ByteReader(std::span<const uint8_t> span) : buf_(span.data()), size_(span.size()), pos_(0) {}

    // Unsigned integers
    uint8_t readU8() { return read<uint8_t>(); }

    uint16_t readU16LE() {
        auto b = readRaw(2);
        return static_cast<uint16_t>(b[0]) | (static_cast<uint16_t>(b[1]) << 8);
    }

    uint16_t readU16BE() {
        auto b = readRaw(2);
        return static_cast<uint16_t>(b[1]) | (static_cast<uint16_t>(b[0]) << 8);
    }

    uint32_t readU32LE() {
        auto b = readRaw(4);
        return static_cast<uint32_t>(b[0]) | (static_cast<uint32_t>(b[1]) << 8) | (static_cast<uint32_t>(b[2]) << 16) |
               (static_cast<uint32_t>(b[3]) << 24);
    }

    uint32_t readU32BE() {
        auto b = readRaw(4);
        return static_cast<uint32_t>(b[3]) | (static_cast<uint32_t>(b[2]) << 8) | (static_cast<uint32_t>(b[1]) << 16) |
               (static_cast<uint32_t>(b[0]) << 24);
    }

    uint64_t readU64LE() {
        auto b = readRaw(8);
        uint64_t v = 0;
        for (int i = 7; i >= 0; --i)
            v = (v << 8) | b[i];
        return v;
    }

    uint64_t readU64BE() {
        auto b = readRaw(8);
        uint64_t v = 0;
        for (int i = 0; i < 8; ++i)
            v = (v << 8) | b[i];
        return v;
    }

    // Signed integers (same wire format, reinterpret)
    int8_t readI8() { return static_cast<int8_t>(readU8()); }
    int16_t readI16LE() { return static_cast<int16_t>(readU16LE()); }
    int16_t readI16BE() { return static_cast<int16_t>(readU16BE()); }
    int32_t readI32LE() { return static_cast<int32_t>(readU32LE()); }
    int32_t readI32BE() { return static_cast<int32_t>(readU32BE()); }
    int64_t readI64LE() { return static_cast<int64_t>(readU64LE()); }
    int64_t readI64BE() { return static_cast<int64_t>(readU64BE()); }

    // Protobuf-style variable-length integer (LEB128 unsigned)
    uint64_t readVarInt() {
        uint64_t result = 0;
        int shift = 0;
        for (;;) {
            if (shift >= 64) {
                throwError("VarInt too long: exceeds 64 bits");
            }
            uint8_t byte = readU8();
            result |= static_cast<uint64_t>(byte & 0x7F) << shift;
            if ((byte & 0x80) == 0)
                return result;
            shift += 7;
        }
    }

    std::span<const uint8_t> readBytes(size_t n) {
        auto ptr = readRaw(n);
        return {ptr, n};
    }

    std::string_view readString(size_t n) {
        auto ptr = readRaw(n);
        return {reinterpret_cast<const char*>(ptr), n}; // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    }

    size_t remaining() const { return size_ - pos_; }
    size_t position() const { return pos_; }

  private:
    template <typename T> T read() {
        auto ptr = readRaw(sizeof(T));
        T val;
        std::memcpy(&val, ptr, sizeof(T));
        return val;
    }

    const uint8_t* readRaw(size_t n) {
        if (pos_ + n > size_) {
            throwError("ByteReader overrun: requested " + std::to_string(n) + " bytes at offset " +
                       std::to_string(pos_) + " with " + std::to_string(size_ - pos_) + " remaining");
        }
        const uint8_t* ptr = buf_ + pos_;
        pos_ += n;
        return ptr;
    }

    const uint8_t* buf_;
    size_t size_;
    size_t pos_;
};

// Binary writer to an internal byte vector.
class ByteWriter {
  public:
    ByteWriter() = default;
    explicit ByteWriter(size_t reserveBytes) { buf_.reserve(reserveBytes); }

    void writeU8(uint8_t v) { buf_.push_back(v); }

    void writeU16LE(uint16_t v) {
        buf_.push_back(static_cast<uint8_t>(v & 0xFF));
        buf_.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    }

    void writeU16BE(uint16_t v) {
        buf_.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        buf_.push_back(static_cast<uint8_t>(v & 0xFF));
    }

    void writeU32LE(uint32_t v) {
        for (int i = 0; i < 4; ++i) {
            buf_.push_back(static_cast<uint8_t>(v & 0xFF));
            v >>= 8;
        }
    }

    void writeU32BE(uint32_t v) {
        for (int i = 3; i >= 0; --i)
            buf_.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFF));
    }

    void writeU64LE(uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            buf_.push_back(static_cast<uint8_t>(v & 0xFF));
            v >>= 8;
        }
    }

    void writeU64BE(uint64_t v) {
        for (int i = 7; i >= 0; --i)
            buf_.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFF));
    }

    // Signed variants
    void writeI8(int8_t v) { writeU8(static_cast<uint8_t>(v)); }
    void writeI16LE(int16_t v) { writeU16LE(static_cast<uint16_t>(v)); }
    void writeI16BE(int16_t v) { writeU16BE(static_cast<uint16_t>(v)); }
    void writeI32LE(int32_t v) { writeU32LE(static_cast<uint32_t>(v)); }
    void writeI32BE(int32_t v) { writeU32BE(static_cast<uint32_t>(v)); }
    void writeI64LE(int64_t v) { writeU64LE(static_cast<uint64_t>(v)); }
    void writeI64BE(int64_t v) { writeU64BE(static_cast<uint64_t>(v)); }

    // Protobuf-style variable-length integer (LEB128 unsigned)
    void writeVarInt(uint64_t v) {
        while (v >= 0x80) {
            buf_.push_back(static_cast<uint8_t>((v & 0x7F) | 0x80));
            v >>= 7;
        }
        buf_.push_back(static_cast<uint8_t>(v));
    }

    void writeBytes(std::span<const uint8_t> data) { buf_.insert(buf_.end(), data.begin(), data.end()); }

    void writeString(std::string_view s) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        buf_.insert(buf_.end(), reinterpret_cast<const uint8_t*>(s.data()),
                    reinterpret_cast<const uint8_t*>(s.data() +
                                                     s.size())); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    }

    const std::vector<uint8_t>& data() const { return buf_; }
    size_t size() const { return buf_.size(); }
    void clear() { buf_.clear(); }

  private:
    std::vector<uint8_t> buf_;
};

// 4-byte little-endian length prefix framing.
// Encode: [len_u32_le][payload]
// Decode: returns payload span if enough data, nullopt otherwise.
class LengthDelimitedFrame {
  public:
    static std::vector<uint8_t> encode(std::span<const uint8_t> payload) {
        std::vector<uint8_t> frame;
        frame.reserve(4 + payload.size());
        uint32_t len = static_cast<uint32_t>(payload.size());
        frame.push_back(static_cast<uint8_t>(len & 0xFF));
        frame.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        frame.push_back(static_cast<uint8_t>((len >> 16) & 0xFF));
        frame.push_back(static_cast<uint8_t>((len >> 24) & 0xFF));
        frame.insert(frame.end(), payload.begin(), payload.end());
        return frame;
    }

    // Incremental decode: returns payload span within buffer if a full frame
    // is available, or nullopt if more data is needed. On success, consumed
    // is set to the total frame bytes (4 + payload length).
    static std::optional<std::span<const uint8_t>> tryDecode(std::span<const uint8_t> buffer, size_t& consumed) {
        if (buffer.size() < 4) {
            consumed = 0;
            return std::nullopt;
        }
        uint32_t len = static_cast<uint32_t>(buffer[0]) | (static_cast<uint32_t>(buffer[1]) << 8) |
                       (static_cast<uint32_t>(buffer[2]) << 16) | (static_cast<uint32_t>(buffer[3]) << 24);
        if (buffer.size() < 4 + len) {
            consumed = 0;
            return std::nullopt;
        }
        consumed = 4 + len;
        return buffer.subspan(4, len);
    }
};

} // namespace fabric::codec
