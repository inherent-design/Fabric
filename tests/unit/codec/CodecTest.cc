#include "fabric/codec/Codec.hh"
#include "fabric/utils/ErrorHandling.hh"
#include <gtest/gtest.h>
#include <cstdint>
#include <vector>

using namespace fabric::codec;
using namespace fabric;

// --- ByteReader tests ---

TEST(ByteReaderTest, ReadUnsignedIntegers) {
  // Layout: u8(0x42) u16le(0x1234) u16be(0x5678)
  std::vector<uint8_t> data = {0x42, 0x34, 0x12, 0x56, 0x78};
  ByteReader reader(data.data(), data.size());

  EXPECT_EQ(reader.readU8(), 0x42);
  EXPECT_EQ(reader.readU16LE(), 0x1234);
  EXPECT_EQ(reader.readU16BE(), 0x5678);
  EXPECT_EQ(reader.remaining(), 0u);
}

TEST(ByteReaderTest, ReadU32) {
  ByteWriter writer;
  writer.writeU32LE(0xDEADBEEF);
  writer.writeU32BE(0xCAFEBABE);

  ByteReader reader(writer.data().data(), writer.data().size());
  EXPECT_EQ(reader.readU32LE(), 0xDEADBEEF);
  EXPECT_EQ(reader.readU32BE(), 0xCAFEBABE);
}

TEST(ByteReaderTest, ReadU64) {
  ByteWriter writer;
  writer.writeU64LE(0x0102030405060708ULL);
  writer.writeU64BE(0x0807060504030201ULL);

  ByteReader reader(writer.data().data(), writer.data().size());
  EXPECT_EQ(reader.readU64LE(), 0x0102030405060708ULL);
  EXPECT_EQ(reader.readU64BE(), 0x0807060504030201ULL);
}

TEST(ByteReaderTest, ReadSignedIntegers) {
  ByteWriter writer;
  writer.writeI8(-1);
  writer.writeI16LE(-300);
  writer.writeI32BE(-100000);

  ByteReader reader(writer.data().data(), writer.data().size());
  EXPECT_EQ(reader.readI8(), -1);
  EXPECT_EQ(reader.readI16LE(), -300);
  EXPECT_EQ(reader.readI32BE(), -100000);
}

TEST(ByteReaderTest, ReadVarInt) {
  ByteWriter writer;
  writer.writeVarInt(0);
  writer.writeVarInt(1);
  writer.writeVarInt(127);
  writer.writeVarInt(128);
  writer.writeVarInt(300);
  writer.writeVarInt(0xFFFFFFFFFFFFFFFFULL);

  ByteReader reader(writer.data().data(), writer.data().size());
  EXPECT_EQ(reader.readVarInt(), 0u);
  EXPECT_EQ(reader.readVarInt(), 1u);
  EXPECT_EQ(reader.readVarInt(), 127u);
  EXPECT_EQ(reader.readVarInt(), 128u);
  EXPECT_EQ(reader.readVarInt(), 300u);
  EXPECT_EQ(reader.readVarInt(), 0xFFFFFFFFFFFFFFFFULL);
}

TEST(ByteReaderTest, ReadBytesAndString) {
  std::vector<uint8_t> payload = {0xAA, 0xBB, 0xCC};
  ByteWriter writer;
  writer.writeBytes(payload);
  writer.writeString("hello");

  ByteReader reader(writer.data().data(), writer.data().size());
  auto bytes = reader.readBytes(3);
  EXPECT_EQ(bytes.size(), 3u);
  EXPECT_EQ(bytes[0], 0xAA);
  EXPECT_EQ(bytes[1], 0xBB);
  EXPECT_EQ(bytes[2], 0xCC);

  auto str = reader.readString(5);
  EXPECT_EQ(str, "hello");
}

TEST(ByteReaderTest, OverrunThrows) {
  std::vector<uint8_t> data = {0x01, 0x02};
  ByteReader reader(data.data(), data.size());

  reader.readU8(); // consume 1 byte
  EXPECT_THROW(reader.readU16LE(), FabricException); // needs 2, only 1 left
}

TEST(ByteReaderTest, PositionTracking) {
  std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
  ByteReader reader(data.data(), data.size());

  EXPECT_EQ(reader.position(), 0u);
  EXPECT_EQ(reader.remaining(), 4u);
  reader.readU8();
  EXPECT_EQ(reader.position(), 1u);
  EXPECT_EQ(reader.remaining(), 3u);
}

TEST(ByteReaderTest, SpanConstructor) {
  std::vector<uint8_t> data = {0xFF};
  std::span<const uint8_t> span(data);
  ByteReader reader(span);
  EXPECT_EQ(reader.readU8(), 0xFF);
}

// --- ByteWriter tests ---

TEST(ByteWriterTest, WriteAndClear) {
  ByteWriter writer;
  writer.writeU8(42);
  EXPECT_EQ(writer.size(), 1u);
  writer.clear();
  EXPECT_EQ(writer.size(), 0u);
}

TEST(ByteWriterTest, RoundTripAllTypes) {
  ByteWriter writer;
  writer.writeU8(0xFF);
  writer.writeU16LE(0xABCD);
  writer.writeU16BE(0x1234);
  writer.writeU32LE(0x12345678);
  writer.writeU32BE(0x87654321);
  writer.writeU64LE(0x0102030405060708ULL);
  writer.writeU64BE(0x0807060504030201ULL);
  writer.writeI8(-42);
  writer.writeI16LE(-1000);
  writer.writeI16BE(-2000);
  writer.writeI32LE(-100000);
  writer.writeI32BE(-200000);
  writer.writeI64LE(-1LL);
  writer.writeI64BE(-2LL);
  writer.writeVarInt(12345);
  writer.writeString("test");

  ByteReader reader(writer.data().data(), writer.data().size());
  EXPECT_EQ(reader.readU8(), 0xFF);
  EXPECT_EQ(reader.readU16LE(), 0xABCD);
  EXPECT_EQ(reader.readU16BE(), 0x1234);
  EXPECT_EQ(reader.readU32LE(), 0x12345678u);
  EXPECT_EQ(reader.readU32BE(), 0x87654321u);
  EXPECT_EQ(reader.readU64LE(), 0x0102030405060708ULL);
  EXPECT_EQ(reader.readU64BE(), 0x0807060504030201ULL);
  EXPECT_EQ(reader.readI8(), -42);
  EXPECT_EQ(reader.readI16LE(), -1000);
  EXPECT_EQ(reader.readI16BE(), -2000);
  EXPECT_EQ(reader.readI32LE(), -100000);
  EXPECT_EQ(reader.readI32BE(), -200000);
  EXPECT_EQ(reader.readI64LE(), -1LL);
  EXPECT_EQ(reader.readI64BE(), -2LL);
  EXPECT_EQ(reader.readVarInt(), 12345u);
  EXPECT_EQ(reader.readString(4), "test");
  EXPECT_EQ(reader.remaining(), 0u);
}

// --- LengthDelimitedFrame tests ---

TEST(LengthDelimitedFrameTest, EncodeDecodeRoundTrip) {
  std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0x04, 0x05};
  auto frame = LengthDelimitedFrame::encode(payload);

  EXPECT_EQ(frame.size(), 4 + payload.size());

  size_t consumed = 0;
  auto decoded = LengthDelimitedFrame::tryDecode(frame, consumed);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(consumed, frame.size());
  EXPECT_EQ(decoded->size(), payload.size());
  for (size_t i = 0; i < payload.size(); ++i) {
    EXPECT_EQ((*decoded)[i], payload[i]);
  }
}

TEST(LengthDelimitedFrameTest, IncompleteDecode) {
  std::vector<uint8_t> payload = {0x01, 0x02, 0x03};
  auto frame = LengthDelimitedFrame::encode(payload);

  // Provide only partial data (header + 1 byte of payload)
  std::span<const uint8_t> partial(frame.data(), 5);
  size_t consumed = 0;
  auto result = LengthDelimitedFrame::tryDecode(partial, consumed);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(consumed, 0u);
}

TEST(LengthDelimitedFrameTest, TooShortForHeader) {
  std::vector<uint8_t> data = {0x01, 0x02};
  size_t consumed = 0;
  auto result = LengthDelimitedFrame::tryDecode(data, consumed);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(consumed, 0u);
}

TEST(LengthDelimitedFrameTest, EmptyPayload) {
  std::vector<uint8_t> empty;
  auto frame = LengthDelimitedFrame::encode(empty);
  EXPECT_EQ(frame.size(), 4u); // just the length prefix

  size_t consumed = 0;
  auto decoded = LengthDelimitedFrame::tryDecode(frame, consumed);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->size(), 0u);
  EXPECT_EQ(consumed, 4u);
}

TEST(LengthDelimitedFrameTest, MultipleFramesInBuffer) {
  std::vector<uint8_t> p1 = {0xAA};
  std::vector<uint8_t> p2 = {0xBB, 0xCC};

  auto f1 = LengthDelimitedFrame::encode(p1);
  auto f2 = LengthDelimitedFrame::encode(p2);

  // Concatenate both frames
  std::vector<uint8_t> combined;
  combined.insert(combined.end(), f1.begin(), f1.end());
  combined.insert(combined.end(), f2.begin(), f2.end());

  // Decode first frame
  size_t consumed = 0;
  auto d1 = LengthDelimitedFrame::tryDecode(combined, consumed);
  ASSERT_TRUE(d1.has_value());
  EXPECT_EQ(d1->size(), 1u);
  EXPECT_EQ((*d1)[0], 0xAA);

  // Decode second frame from remaining buffer
  auto remaining = std::span<const uint8_t>(combined).subspan(consumed);
  size_t consumed2 = 0;
  auto d2 = LengthDelimitedFrame::tryDecode(remaining, consumed2);
  ASSERT_TRUE(d2.has_value());
  EXPECT_EQ(d2->size(), 2u);
  EXPECT_EQ((*d2)[0], 0xBB);
  EXPECT_EQ((*d2)[1], 0xCC);
}
