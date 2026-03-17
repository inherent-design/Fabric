#include "recurse/persistence/FchkCodec.hh"
#include "fabric/utils/ErrorHandling.hh"
#include "recurse/simulation/VoxelMaterial.hh"
#include <algorithm>
#include <cstring>
#include <gtest/gtest.h>
#include <numeric>
#include <random>

using namespace recurse;

class FchkCodecTest : public ::testing::Test {
  protected:
    static constexpr size_t K_CELL_COUNT = 32 * 32 * 32;
    static constexpr size_t K_CELLS_BYTE_COUNT = K_CELL_COUNT * 4;

    static std::vector<uint8_t> makeEmptyChunk() { return std::vector<uint8_t>(K_CELLS_BYTE_COUNT, 0); }

    static std::vector<uint8_t> makeUniformChunk(uint16_t materialId, uint8_t essenceIdx, uint8_t flags) {
        std::vector<uint8_t> cells(K_CELLS_BYTE_COUNT);
        for (size_t i = 0; i < K_CELL_COUNT; ++i) {
            size_t base = i * 4;
            std::memcpy(&cells[base], &materialId, 2);
            cells[base + 2] = essenceIdx;
            cells[base + 3] = flags;
        }
        return cells;
    }

    static std::vector<uint8_t> makeRandomChunk(uint32_t seed = 42) {
        std::mt19937 rng(seed);
        std::vector<uint8_t> cells(K_CELLS_BYTE_COUNT);
        std::generate(cells.begin(), cells.end(), [&]() { return static_cast<uint8_t>(rng() & 0xFF); });
        return cells;
    }

    static std::vector<float> makeSamplePalette() {
        return {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 0.0f, 0.05f};
    }

    static std::vector<uint8_t> expectedCells(const std::vector<uint8_t>& cells) {
        auto expected = cells;
        constexpr uint8_t kMask = static_cast<uint8_t>(~(0x01 | 0x02));
        for (size_t i = 3; i < expected.size(); i += 4) {
            expected[i] &= kMask;
        }
        return expected;
    }

    static void verifyRoundTrip(const std::vector<uint8_t>& cells, uint8_t compression, const float* palette = nullptr,
                                uint16_t paletteCount = 0) {
        auto blob = FchkCodec::encode(cells.data(), cells.size(), compression, 1, palette, paletteCount);
        ASSERT_FALSE(blob.empty());

        auto decoded = FchkCodec::decode(blob);
        auto expected = expectedCells(cells);
        ASSERT_EQ(decoded.cells.size(), expected.size());
        EXPECT_TRUE(std::equal(decoded.cells.begin(), decoded.cells.end(), expected.begin()))
            << "Cell data mismatch for compression=" << static_cast<int>(compression);

        EXPECT_EQ(decoded.paletteEntryCount, paletteCount);
        if (paletteCount > 0 && palette) {
            ASSERT_EQ(decoded.paletteData.size(), static_cast<size_t>(paletteCount) * 4);
            for (size_t i = 0; i < decoded.paletteData.size(); ++i) {
                EXPECT_FLOAT_EQ(decoded.paletteData[i], palette[i]) << "palette float " << i;
            }
        }
    }
};

TEST_F(FchkCodecTest, UncompressedEmptyChunk) {
    verifyRoundTrip(makeEmptyChunk(), 0);
}

TEST_F(FchkCodecTest, ZstdEmptyChunk) {
    verifyRoundTrip(makeEmptyChunk(), 1);
}

TEST_F(FchkCodecTest, Lz4EmptyChunk) {
    verifyRoundTrip(makeEmptyChunk(), 2);
}

TEST_F(FchkCodecTest, ZstdUniformChunk) {
    verifyRoundTrip(makeUniformChunk(simulation::material_ids::STONE, 0, 0), 1);
}

TEST_F(FchkCodecTest, Lz4UniformChunk) {
    verifyRoundTrip(makeUniformChunk(simulation::material_ids::STONE, 0, 0), 2);
}

TEST_F(FchkCodecTest, ZstdRandomChunk) {
    verifyRoundTrip(makeRandomChunk(), 1);
}

TEST_F(FchkCodecTest, Lz4RandomChunk) {
    verifyRoundTrip(makeRandomChunk(), 2);
}

TEST_F(FchkCodecTest, ZstdWithPalette) {
    auto pal = makeSamplePalette();
    verifyRoundTrip(makeUniformChunk(simulation::material_ids::STONE, 1, 0), 1, pal.data(), 3);
}

TEST_F(FchkCodecTest, Lz4WithPalette) {
    auto pal = makeSamplePalette();
    verifyRoundTrip(makeUniformChunk(simulation::material_ids::STONE, 1, 0), 2, pal.data(), 3);
}

TEST_F(FchkCodecTest, UncompressedWithPalette) {
    auto pal = makeSamplePalette();
    verifyRoundTrip(makeUniformChunk(simulation::material_ids::STONE, 1, 0), 0, pal.data(), 3);
}

TEST_F(FchkCodecTest, ZstdMaxPaletteEntries) {
    std::vector<float> largePalette(256 * 4);
    std::mt19937 rng(99);
    std::generate(largePalette.begin(), largePalette.end(),
                  [&]() { return static_cast<float>(rng() % 1000) / 1000.0f; });
    verifyRoundTrip(makeRandomChunk(), 1, largePalette.data(), 256);
}

TEST_F(FchkCodecTest, CompressedBlobSmaller) {
    auto cells = makeUniformChunk(simulation::material_ids::STONE, 0, 0);
    auto uncompressed = FchkCodec::encode(cells.data(), cells.size(), 0);
    auto zstd = FchkCodec::encode(cells.data(), cells.size(), 1);
    EXPECT_LT(zstd.size(), uncompressed.size());
}

TEST_F(FchkCodecTest, RuntimeFlagsClearedOnDecode) {
    auto cells = makeUniformChunk(simulation::material_ids::SAND, 1,
                                  simulation::voxel_flags::UPDATED | simulation::voxel_flags::FREE_FALL);

    for (uint8_t comp : {0, 1, 2}) {
        auto blob = FchkCodec::encode(cells.data(), cells.size(), comp);
        auto decoded = FchkCodec::decode(blob);
        for (size_t i = 3; i < decoded.cells.size(); i += 4) {
            EXPECT_EQ(decoded.cells[i] & 0x03, 0)
                << "Runtime flags not cleared at offset " << i << " for compression=" << static_cast<int>(comp);
        }
    }
}

// --- v3 delta tests ---

static std::vector<uint8_t> applyDelta(const std::vector<uint8_t>& reference, const FchkDeltaDecoded& delta) {
    auto result = reference;
    for (const auto& e : delta.entries) {
        size_t offset = static_cast<size_t>(e.cellIndex) * 4;
        std::memcpy(&result[offset], &e.cellData, 4);
    }
    return result;
}

TEST_F(FchkCodecTest, DeltaZeroDiffs) {
    auto cells = makeUniformChunk(simulation::material_ids::STONE, 0, 0);
    auto blob = FchkCodec::encodeDelta(cells.data(), cells.data(), cells.size(), 0xABCD, 0);
    auto delta = FchkCodec::decodeDelta(blob);
    EXPECT_EQ(delta.entries.size(), 0u);
    EXPECT_EQ(delta.worldgenVersion, 0xABCDu);
}

TEST_F(FchkCodecTest, DeltaPartialDiffs) {
    auto reference = makeUniformChunk(simulation::material_ids::STONE, 0, 0);
    auto current = reference;

    // Modify 5% of cells (~1638 cells)
    std::mt19937 rng(123);
    size_t modCount = K_CELL_COUNT / 20;
    std::vector<size_t> indices(K_CELL_COUNT);
    std::iota(indices.begin(), indices.end(), 0);
    std::shuffle(indices.begin(), indices.end(), rng);

    for (size_t i = 0; i < modCount; ++i) {
        size_t base = indices[i] * 4;
        uint16_t newMat = simulation::material_ids::SAND;
        std::memcpy(&current[base], &newMat, 2);
        current[base + 2] = 1;
        current[base + 3] = 0;
    }

    auto blob = FchkCodec::encodeDelta(current.data(), reference.data(), current.size(), 42, 0);
    auto delta = FchkCodec::decodeDelta(blob);
    EXPECT_EQ(delta.entries.size(), modCount);

    auto applied = applyDelta(expectedCells(reference), delta);
    auto expected = expectedCells(current);
    EXPECT_EQ(applied, expected);
}

TEST_F(FchkCodecTest, DeltaAllDiffs) {
    auto reference = makeUniformChunk(simulation::material_ids::STONE, 0, 0);
    auto current = makeRandomChunk(77);

    auto blob = FchkCodec::encodeDelta(current.data(), reference.data(), current.size(), 99, 0);
    auto delta = FchkCodec::decodeDelta(blob);
    EXPECT_EQ(delta.entries.size(), K_CELL_COUNT);

    auto applied = applyDelta(expectedCells(reference), delta);
    auto expected = expectedCells(current);
    EXPECT_EQ(applied, expected);
}

TEST_F(FchkCodecTest, DeltaZstdCompression) {
    auto reference = makeUniformChunk(simulation::material_ids::STONE, 0, 0);
    auto current = reference;
    uint16_t newMat = simulation::material_ids::SAND;
    std::memcpy(&current[0], &newMat, 2);

    auto blob = FchkCodec::encodeDelta(current.data(), reference.data(), current.size(), 1, 1);
    auto delta = FchkCodec::decodeDelta(blob);
    EXPECT_EQ(delta.entries.size(), 1u);

    auto applied = applyDelta(expectedCells(reference), delta);
    auto expected = expectedCells(current);
    EXPECT_EQ(applied, expected);
}

TEST_F(FchkCodecTest, DeltaLz4Compression) {
    auto reference = makeUniformChunk(simulation::material_ids::STONE, 0, 0);
    auto current = reference;
    uint16_t newMat = simulation::material_ids::SAND;
    std::memcpy(&current[0], &newMat, 2);

    auto blob = FchkCodec::encodeDelta(current.data(), reference.data(), current.size(), 1, 2);
    auto delta = FchkCodec::decodeDelta(blob);
    EXPECT_EQ(delta.entries.size(), 1u);

    auto applied = applyDelta(expectedCells(reference), delta);
    auto expected = expectedCells(current);
    EXPECT_EQ(applied, expected);
}

TEST_F(FchkCodecTest, DeltaWithPalette) {
    auto reference = makeUniformChunk(simulation::material_ids::STONE, 0, 0);
    auto current = reference;
    uint16_t newMat = simulation::material_ids::SAND;
    std::memcpy(&current[0], &newMat, 2);
    current[2] = 1;

    auto pal = makeSamplePalette();
    auto blob = FchkCodec::encodeDelta(current.data(), reference.data(), current.size(), 55, 1, 1, pal.data(), 3);
    auto delta = FchkCodec::decodeDelta(blob);
    EXPECT_EQ(delta.paletteEntryCount, 3u);
    ASSERT_EQ(delta.paletteData.size(), 12u);
    for (size_t i = 0; i < delta.paletteData.size(); ++i) {
        EXPECT_FLOAT_EQ(delta.paletteData[i], pal[i]) << "palette float " << i;
    }
}

TEST_F(FchkCodecTest, DeltaIsDelta) {
    auto cells = makeUniformChunk(simulation::material_ids::STONE, 0, 0);
    auto v3 = FchkCodec::encodeDelta(cells.data(), cells.data(), cells.size(), 0, 1);
    auto v2 = FchkCodec::encode(cells.data(), cells.size(), 1);

    EXPECT_TRUE(FchkCodec::isDelta(v3));
    EXPECT_FALSE(FchkCodec::isDelta(v2));
}

TEST_F(FchkCodecTest, DeltaDecodeRejectsV2) {
    auto cells = makeUniformChunk(simulation::material_ids::STONE, 0, 0);
    auto v3 = FchkCodec::encodeDelta(cells.data(), cells.data(), cells.size(), 0, 0);
    EXPECT_THROW(FchkCodec::decode(v3), fabric::FabricException);
}

TEST_F(FchkCodecTest, DeltaWorldgenVersion) {
    auto cells = makeUniformChunk(simulation::material_ids::STONE, 0, 0);
    constexpr uint32_t version = 0xDEADBEEF;
    auto blob = FchkCodec::encodeDelta(cells.data(), cells.data(), cells.size(), version, 1);
    auto delta = FchkCodec::decodeDelta(blob);
    EXPECT_EQ(delta.worldgenVersion, version);
}

TEST_F(FchkCodecTest, DeltaSmallerThanFull) {
    auto reference = makeUniformChunk(simulation::material_ids::STONE, 0, 0);
    auto current = reference;

    // Modify 5% of cells
    std::mt19937 rng(456);
    size_t modCount = K_CELL_COUNT / 20;
    std::vector<size_t> indices(K_CELL_COUNT);
    std::iota(indices.begin(), indices.end(), 0);
    std::shuffle(indices.begin(), indices.end(), rng);

    for (size_t i = 0; i < modCount; ++i) {
        size_t base = indices[i] * 4;
        uint16_t newMat = simulation::material_ids::SAND;
        std::memcpy(&current[base], &newMat, 2);
    }

    auto deltaBlob = FchkCodec::encodeDelta(current.data(), reference.data(), current.size(), 0, 1);
    auto fullBlob = FchkCodec::encode(current.data(), current.size(), 1);
    EXPECT_LT(deltaBlob.size(), fullBlob.size())
        << "Delta blob (" << deltaBlob.size() << ") should be smaller than full blob (" << fullBlob.size() << ")";
}
