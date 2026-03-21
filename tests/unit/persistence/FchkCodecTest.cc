#include "recurse/persistence/FchkCodec.hh"
#include "fabric/utils/ErrorHandling.hh"
#include "recurse/simulation/CellAccessors.hh"
#include "recurse/simulation/MatterState.hh"
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

    /// Build a uniform chunk in new v4 layout: [essenceIdx, displacementRank, phaseAndFlags, spare].
    /// essenceIdx == materialId during migration. Phase and displacementRank are derived from materialId.
    /// flags occupy bits 3-7 of phaseAndFlags (shifted left by 3).
    static std::vector<uint8_t> makeUniformChunk(uint16_t materialId, uint8_t /*essenceIdx*/, uint8_t flags) {
        using namespace simulation;
        uint8_t essence = static_cast<uint8_t>(materialId);
        uint8_t phase = 0;
        uint8_t density = 0;
        switch (materialId) {
            case material_ids::AIR:
                phase = 0;
                density = 0;
                break;
            case material_ids::STONE:
                phase = 1;
                density = 200;
                break;
            case material_ids::DIRT:
                phase = 1;
                density = 150;
                break;
            case material_ids::SAND:
                phase = 2;
                density = 130;
                break;
            case material_ids::WATER:
                phase = 3;
                density = 100;
                break;
            case material_ids::GRAVEL:
                phase = 2;
                density = 170;
                break;
            default:
                phase = 1;
                density = 128;
                break;
        }
        uint8_t phaseAndFlags = static_cast<uint8_t>((phase & 0x07) | ((flags & 0x1F) << 3));
        std::vector<uint8_t> cells(K_CELLS_BYTE_COUNT);
        for (size_t i = 0; i < K_CELL_COUNT; ++i) {
            size_t base = i * 4;
            cells[base + 0] = essence;       // essenceIdx
            cells[base + 1] = density;       // displacementRank
            cells[base + 2] = phaseAndFlags; // phase | (flags << 3)
            cells[base + 3] = 0;             // spare
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

    /// Expected cells after v4 decode: runtime flags (bits 3-4 of phaseAndFlags, byte 2) cleared.
    static std::vector<uint8_t> expectedCells(const std::vector<uint8_t>& cells) {
        auto expected = cells;
        constexpr uint8_t kMask = static_cast<uint8_t>(~(0x08 | 0x10));
        for (size_t i = 2; i < expected.size(); i += 4) {
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
        // v4 layout: runtime flags are bits 3-4 of phaseAndFlags (byte 2 of each cell)
        for (size_t i = 2; i < decoded.cells.size(); i += 4) {
            EXPECT_EQ(decoded.cells[i] & 0x18, 0)
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
        // v4 layout: [essenceIdx, displacementRank, phaseAndFlags, spare]
        current[base + 0] = static_cast<uint8_t>(simulation::material_ids::SAND); // essenceIdx
        current[base + 1] = 130;                                                  // displacementRank for SAND
        current[base + 2] = 2;                                                    // Phase::Powder, no flags
        current[base + 3] = 0;                                                    // spare
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
    // v4 layout: change cell 0 to SAND
    current[0] = static_cast<uint8_t>(simulation::material_ids::SAND);
    current[1] = 130; // displacementRank for SAND
    current[2] = 2;   // Phase::Powder

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
    // v4 layout: change cell 0 to SAND
    current[0] = static_cast<uint8_t>(simulation::material_ids::SAND);
    current[1] = 130; // displacementRank for SAND
    current[2] = 2;   // Phase::Powder

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
    // v4 layout: change cell 0 to SAND
    current[0] = static_cast<uint8_t>(simulation::material_ids::SAND);
    current[1] = 130; // displacementRank for SAND
    current[2] = 2;   // Phase::Powder

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
        // v4 layout: change to SAND
        current[base + 0] = static_cast<uint8_t>(simulation::material_ids::SAND);
        current[base + 1] = 130;
        current[base + 2] = 2;
        current[base + 3] = 0;
    }

    auto deltaBlob = FchkCodec::encodeDelta(current.data(), reference.data(), current.size(), 0, 1);
    auto fullBlob = FchkCodec::encode(current.data(), current.size(), 1);
    EXPECT_LT(deltaBlob.size(), fullBlob.size())
        << "Delta blob (" << deltaBlob.size() << ") should be smaller than full blob (" << fullBlob.size() << ")";
}

// --- v4 MatterState layout tests ---

class FchkCodecV4Test : public ::testing::Test {
  protected:
    static constexpr size_t K_CELL_COUNT = 32 * 32 * 32;
    static constexpr size_t K_CELLS_BYTE_COUNT = K_CELL_COUNT * 4;

    using Phase = simulation::Phase;
    using MatterState = simulation::MatterState;

    /// Build a chunk of uniform MatterState cells in raw byte form.
    static std::vector<uint8_t> makeMatterChunk(uint8_t essenceIdx, Phase phase, uint8_t displacementRank = 0,
                                                uint8_t flags = 0) {
        MatterState cell;
        cell.essenceIdx = essenceIdx;
        cell.displacementRank = displacementRank;
        cell.setPhase(phase);
        cell.setFlags(flags);
        std::vector<uint8_t> cells(K_CELLS_BYTE_COUNT);
        for (size_t i = 0; i < K_CELL_COUNT; ++i) {
            std::memcpy(&cells[i * 4], &cell, 4);
        }
        return cells;
    }

    /// Patch a blob's header version from 2 to a target version.
    /// FchkHeader layout: magic[4] + version[2] (LE) + dims[3] + compression[1].
    static void patchVersion(ChunkBlob& blob, uint16_t version) {
        std::memcpy(blob.data_ptr() + 4, &version, sizeof(uint16_t));
    }

    /// Encode cells as uncompressed v2 blob, then patch header to v4.
    static ChunkBlob encodeAsV4(const void* cells, size_t cellsByteCount, uint8_t compression = 0, int level = 1,
                                const float* paletteData = nullptr, uint16_t paletteEntryCount = 0) {
        auto blob = FchkCodec::encode(cells, cellsByteCount, compression, level, paletteData, paletteEntryCount);
        patchVersion(blob, 4);
        return blob;
    }

    /// Expected cells after v4 decode: runtime flags (bits 3-4 of phaseAndFlags, byte 2) cleared.
    static std::vector<uint8_t> expectedV4Cells(const std::vector<uint8_t>& cells) {
        auto expected = cells;
        constexpr uint8_t kMask = static_cast<uint8_t>(~(0x08 | 0x10));
        for (size_t i = 2; i < expected.size(); i += 4) {
            expected[i] &= kMask;
        }
        return expected;
    }

    static std::vector<float> makeSamplePalette() {
        return {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 0.0f, 0.05f};
    }
};

TEST_F(FchkCodecV4Test, RoundTrip) {
    auto cells = makeMatterChunk(1, Phase::Solid, 128);
    auto blob = encodeAsV4(cells.data(), cells.size());

    auto decoded = FchkCodec::decode(blob);
    auto expected = expectedV4Cells(cells);
    ASSERT_EQ(decoded.cells.size(), expected.size());
    EXPECT_TRUE(std::equal(decoded.cells.begin(), decoded.cells.end(), expected.begin()))
        << "v4 round-trip cell data mismatch";
}

TEST_F(FchkCodecV4Test, RuntimeFlagsCleared) {
    // Set UPDATED (flags bit 0 -> phaseAndFlags bit 3) and FREE_FALL (flags bit 1 -> phaseAndFlags bit 4)
    constexpr uint8_t kUpdated = 1;  // flags bit 0
    constexpr uint8_t kFreeFall = 2; // flags bit 1
    auto cells = makeMatterChunk(2, Phase::Powder, 64, kUpdated | kFreeFall);

    // Verify the raw phaseAndFlags byte has bits 3-4 set
    MatterState probe;
    std::memcpy(&probe, &cells[0], 4);
    ASSERT_NE(probe.phaseAndFlags & 0x18, 0) << "Test setup: runtime flags should be set in phaseAndFlags";

    auto blob = encodeAsV4(cells.data(), cells.size());
    auto decoded = FchkCodec::decode(blob);

    for (size_t i = 0; i < K_CELL_COUNT; ++i) {
        size_t base = i * 4;
        MatterState decodedCell;
        std::memcpy(&decodedCell, &decoded.cells[base], 4);

        // Phase (low 3 bits) must be preserved
        EXPECT_EQ(decodedCell.phase(), Phase::Powder) << "Phase lost at cell " << i;

        // Runtime flags (bits 3-4 of phaseAndFlags) must be cleared
        EXPECT_EQ(decodedCell.phaseAndFlags & 0x18, 0) << "Runtime flags not cleared at cell " << i;

        // essenceIdx and displacementRank untouched
        EXPECT_EQ(decodedCell.essenceIdx, 2) << "essenceIdx corrupted at cell " << i;
        EXPECT_EQ(decodedCell.displacementRank, 64) << "displacementRank corrupted at cell " << i;
    }
}

TEST_F(FchkCodecV4Test, PaletteRoundTrip) {
    auto cells = makeMatterChunk(1, Phase::Solid, 128);
    auto pal = makeSamplePalette();

    auto blob = encodeAsV4(cells.data(), cells.size(), 0, 1, pal.data(), 3);
    auto decoded = FchkCodec::decode(blob);

    EXPECT_EQ(decoded.paletteEntryCount, 3u);
    ASSERT_EQ(decoded.paletteData.size(), 12u);
    for (size_t i = 0; i < decoded.paletteData.size(); ++i) {
        EXPECT_FLOAT_EQ(decoded.paletteData[i], pal[i]) << "palette float " << i;
    }
}

TEST_F(FchkCodecV4Test, WithZstd) {
    auto cells = makeMatterChunk(3, Phase::Liquid, 32);
    auto blob = encodeAsV4(cells.data(), cells.size(), 1);

    auto decoded = FchkCodec::decode(blob);
    auto expected = expectedV4Cells(cells);
    ASSERT_EQ(decoded.cells.size(), expected.size());
    EXPECT_TRUE(std::equal(decoded.cells.begin(), decoded.cells.end(), expected.begin()))
        << "v4 zstd round-trip cell data mismatch";
}

TEST_F(FchkCodecV4Test, WithLz4) {
    auto cells = makeMatterChunk(4, Phase::Gas, 16);
    auto blob = encodeAsV4(cells.data(), cells.size(), 2);

    auto decoded = FchkCodec::decode(blob);
    auto expected = expectedV4Cells(cells);
    ASSERT_EQ(decoded.cells.size(), expected.size());
    EXPECT_TRUE(std::equal(decoded.cells.begin(), decoded.cells.end(), expected.begin()))
        << "v4 LZ4 round-trip cell data mismatch";
}

TEST_F(FchkCodecV4Test, PhasePreservedAcrossAllValues) {
    // Test every Phase enum value round-trips through v4 decode
    for (uint8_t p = 0; p <= 4; ++p) {
        auto phase = static_cast<Phase>(p);
        auto cells = makeMatterChunk(p + 1, phase, p * 50);

        auto blob = encodeAsV4(cells.data(), cells.size());
        auto decoded = FchkCodec::decode(blob);

        MatterState decodedCell;
        std::memcpy(&decodedCell, &decoded.cells[0], 4);
        EXPECT_EQ(decodedCell.phase(), phase) << "Phase not preserved for Phase=" << static_cast<int>(p);
        EXPECT_EQ(decodedCell.essenceIdx, p + 1) << "essenceIdx corrupted for Phase=" << static_cast<int>(p);
        EXPECT_EQ(decodedCell.displacementRank, p * 50)
            << "displacementRank corrupted for Phase=" << static_cast<int>(p);
    }
}

TEST_F(FchkCodecV4Test, DecodeAnyHandlesV4) {
    auto cells = makeMatterChunk(1, Phase::Solid, 128);
    auto blob = encodeAsV4(cells.data(), cells.size());

    // decodeAny delegates to decode for non-v3 blobs
    auto decoded = FchkCodec::decodeAny(blob);
    auto expected = expectedV4Cells(cells);
    ASSERT_EQ(decoded.cells.size(), expected.size());
    EXPECT_TRUE(std::equal(decoded.cells.begin(), decoded.cells.end(), expected.begin()))
        << "v4 decodeAny cell data mismatch";
}

TEST_F(FchkCodecV4Test, UnsupportedVersionRejected) {
    auto cells = makeMatterChunk(1, Phase::Solid);
    auto blob = encodeAsV4(cells.data(), cells.size());
    patchVersion(blob, 5);
    EXPECT_THROW(FchkCodec::decode(blob), fabric::FabricException);
}
