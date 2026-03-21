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

    using Phase = simulation::Phase;
    using MatterState = simulation::MatterState;

    static std::vector<uint8_t> makeEmptyChunk() { return std::vector<uint8_t>(K_CELLS_BYTE_COUNT, 0); }

    /// Build a uniform chunk in MatterState layout: [essenceIdx, displacementRank, phaseAndFlags, spare].
    static std::vector<uint8_t> makeUniformChunk(uint16_t materialId, uint8_t flags = 0) {
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
            cells[base + 0] = essence;
            cells[base + 1] = density;
            cells[base + 2] = phaseAndFlags;
            cells[base + 3] = 0;
        }
        return cells;
    }

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

    static std::vector<uint8_t> makeRandomChunk(uint32_t seed = 42) {
        std::mt19937 rng(seed);
        std::vector<uint8_t> cells(K_CELLS_BYTE_COUNT);
        std::generate(cells.begin(), cells.end(), [&]() { return static_cast<uint8_t>(rng() & 0xFF); });
        return cells;
    }

    static std::vector<float> makeSamplePalette() {
        return {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 0.0f, 0.05f};
    }

    /// Expected cells after decode: runtime flags (bits 3-4 of phaseAndFlags, byte 2) cleared.
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

    /// Patch a blob's header version field (for testing version rejection).
    static void patchVersion(ChunkBlob& blob, uint16_t version) {
        std::memcpy(blob.data_ptr() + 4, &version, sizeof(uint16_t));
    }
};

// --- v1 full snapshot tests ---

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
    verifyRoundTrip(makeUniformChunk(simulation::material_ids::STONE), 1);
}

TEST_F(FchkCodecTest, Lz4UniformChunk) {
    verifyRoundTrip(makeUniformChunk(simulation::material_ids::STONE), 2);
}

TEST_F(FchkCodecTest, ZstdRandomChunk) {
    verifyRoundTrip(makeRandomChunk(), 1);
}

TEST_F(FchkCodecTest, Lz4RandomChunk) {
    verifyRoundTrip(makeRandomChunk(), 2);
}

TEST_F(FchkCodecTest, ZstdWithPalette) {
    auto pal = makeSamplePalette();
    verifyRoundTrip(makeUniformChunk(simulation::material_ids::STONE), 1, pal.data(), 3);
}

TEST_F(FchkCodecTest, Lz4WithPalette) {
    auto pal = makeSamplePalette();
    verifyRoundTrip(makeUniformChunk(simulation::material_ids::STONE), 2, pal.data(), 3);
}

TEST_F(FchkCodecTest, UncompressedWithPalette) {
    auto pal = makeSamplePalette();
    verifyRoundTrip(makeUniformChunk(simulation::material_ids::STONE), 0, pal.data(), 3);
}

TEST_F(FchkCodecTest, ZstdMaxPaletteEntries) {
    std::vector<float> largePalette(256 * 4);
    std::mt19937 rng(99);
    std::generate(largePalette.begin(), largePalette.end(),
                  [&]() { return static_cast<float>(rng() % 1000) / 1000.0f; });
    verifyRoundTrip(makeRandomChunk(), 1, largePalette.data(), 256);
}

TEST_F(FchkCodecTest, CompressedBlobSmaller) {
    auto cells = makeUniformChunk(simulation::material_ids::STONE);
    auto uncompressed = FchkCodec::encode(cells.data(), cells.size(), 0);
    auto zstd = FchkCodec::encode(cells.data(), cells.size(), 1);
    EXPECT_LT(zstd.size(), uncompressed.size());
}

TEST_F(FchkCodecTest, RuntimeFlagsClearedOnDecode) {
    auto cells = makeUniformChunk(simulation::material_ids::SAND,
                                  simulation::voxel_flags::UPDATED | simulation::voxel_flags::FREE_FALL);

    for (uint8_t comp : {0, 1, 2}) {
        auto blob = FchkCodec::encode(cells.data(), cells.size(), comp);
        auto decoded = FchkCodec::decode(blob);
        for (size_t i = 2; i < decoded.cells.size(); i += 4) {
            EXPECT_EQ(decoded.cells[i] & 0x18, 0)
                << "Runtime flags not cleared at offset " << i << " for compression=" << static_cast<int>(comp);
        }
    }
}

TEST_F(FchkCodecTest, MatterStateRoundTrip) {
    auto cells = makeMatterChunk(1, Phase::Solid, 128);
    auto blob = FchkCodec::encode(cells.data(), cells.size());

    auto decoded = FchkCodec::decode(blob);
    auto expected = expectedCells(cells);
    ASSERT_EQ(decoded.cells.size(), expected.size());
    EXPECT_TRUE(std::equal(decoded.cells.begin(), decoded.cells.end(), expected.begin()));
}

TEST_F(FchkCodecTest, MatterStateRuntimeFlagsCleared) {
    constexpr uint8_t kUpdated = 1;
    constexpr uint8_t kFreeFall = 2;
    auto cells = makeMatterChunk(2, Phase::Powder, 64, kUpdated | kFreeFall);

    MatterState probe;
    std::memcpy(&probe, &cells[0], 4);
    ASSERT_NE(probe.phaseAndFlags & 0x18, 0) << "Test setup: runtime flags should be set";

    auto blob = FchkCodec::encode(cells.data(), cells.size());
    auto decoded = FchkCodec::decode(blob);

    for (size_t i = 0; i < K_CELL_COUNT; ++i) {
        size_t base = i * 4;
        MatterState decodedCell;
        std::memcpy(&decodedCell, &decoded.cells[base], 4);

        EXPECT_EQ(decodedCell.phase(), Phase::Powder) << "Phase lost at cell " << i;
        EXPECT_EQ(decodedCell.phaseAndFlags & 0x18, 0) << "Runtime flags not cleared at cell " << i;
        EXPECT_EQ(decodedCell.essenceIdx, 2) << "essenceIdx corrupted at cell " << i;
        EXPECT_EQ(decodedCell.displacementRank, 64) << "displacementRank corrupted at cell " << i;
    }
}

TEST_F(FchkCodecTest, MatterStatePaletteRoundTrip) {
    auto cells = makeMatterChunk(1, Phase::Solid, 128);
    auto pal = makeSamplePalette();

    auto blob = FchkCodec::encode(cells.data(), cells.size(), 0, 1, pal.data(), 3);
    auto decoded = FchkCodec::decode(blob);

    EXPECT_EQ(decoded.paletteEntryCount, 3u);
    ASSERT_EQ(decoded.paletteData.size(), 12u);
    for (size_t i = 0; i < decoded.paletteData.size(); ++i) {
        EXPECT_FLOAT_EQ(decoded.paletteData[i], pal[i]) << "palette float " << i;
    }
}

TEST_F(FchkCodecTest, MatterStateWithZstd) {
    auto cells = makeMatterChunk(3, Phase::Liquid, 32);
    auto blob = FchkCodec::encode(cells.data(), cells.size(), 1);

    auto decoded = FchkCodec::decode(blob);
    auto expected = expectedCells(cells);
    ASSERT_EQ(decoded.cells.size(), expected.size());
    EXPECT_TRUE(std::equal(decoded.cells.begin(), decoded.cells.end(), expected.begin()));
}

TEST_F(FchkCodecTest, MatterStateWithLz4) {
    auto cells = makeMatterChunk(4, Phase::Gas, 16);
    auto blob = FchkCodec::encode(cells.data(), cells.size(), 2);

    auto decoded = FchkCodec::decode(blob);
    auto expected = expectedCells(cells);
    ASSERT_EQ(decoded.cells.size(), expected.size());
    EXPECT_TRUE(std::equal(decoded.cells.begin(), decoded.cells.end(), expected.begin()));
}

TEST_F(FchkCodecTest, PhasePreservedAcrossAllValues) {
    for (uint8_t p = 0; p <= 4; ++p) {
        auto phase = static_cast<Phase>(p);
        auto cells = makeMatterChunk(p + 1, phase, p * 50);

        auto blob = FchkCodec::encode(cells.data(), cells.size());
        auto decoded = FchkCodec::decode(blob);

        MatterState decodedCell;
        std::memcpy(&decodedCell, &decoded.cells[0], 4);
        EXPECT_EQ(decodedCell.phase(), phase) << "Phase not preserved for Phase=" << static_cast<int>(p);
        EXPECT_EQ(decodedCell.essenceIdx, p + 1) << "essenceIdx corrupted for Phase=" << static_cast<int>(p);
        EXPECT_EQ(decodedCell.displacementRank, p * 50)
            << "displacementRank corrupted for Phase=" << static_cast<int>(p);
    }
}

TEST_F(FchkCodecTest, DecodeAnyHandlesV1) {
    auto cells = makeMatterChunk(1, Phase::Solid, 128);
    auto blob = FchkCodec::encode(cells.data(), cells.size());

    auto decoded = FchkCodec::decodeAny(blob);
    auto expected = expectedCells(cells);
    ASSERT_EQ(decoded.cells.size(), expected.size());
    EXPECT_TRUE(std::equal(decoded.cells.begin(), decoded.cells.end(), expected.begin()));
}

TEST_F(FchkCodecTest, UnsupportedVersionRejected) {
    auto cells = makeMatterChunk(1, Phase::Solid);
    auto blob = FchkCodec::encode(cells.data(), cells.size());
    patchVersion(blob, 5);
    EXPECT_THROW(FchkCodec::decode(blob), fabric::FabricException);
}

TEST_F(FchkCodecTest, V3VersionRejected) {
    auto cells = makeMatterChunk(1, Phase::Solid);
    auto blob = FchkCodec::encode(cells.data(), cells.size());
    patchVersion(blob, 3);
    EXPECT_THROW(FchkCodec::decode(blob), fabric::FabricException);
}

TEST_F(FchkCodecTest, V4VersionRejected) {
    auto cells = makeMatterChunk(1, Phase::Solid);
    auto blob = FchkCodec::encode(cells.data(), cells.size());
    patchVersion(blob, 4);
    EXPECT_THROW(FchkCodec::decode(blob), fabric::FabricException);
}

// --- v2 delta tests ---

static std::vector<uint8_t> applyDelta(const std::vector<uint8_t>& reference, const FchkDeltaDecoded& delta) {
    auto result = reference;
    for (const auto& e : delta.entries) {
        size_t offset = static_cast<size_t>(e.cellIndex) * 4;
        std::memcpy(&result[offset], &e.cellData, 4);
    }
    return result;
}

TEST_F(FchkCodecTest, DeltaZeroDiffs) {
    auto cells = makeUniformChunk(simulation::material_ids::STONE);
    auto blob = FchkCodec::encodeDelta(cells.data(), cells.data(), cells.size(), 0xABCD, 0);
    auto delta = FchkCodec::decodeDelta(blob);
    EXPECT_EQ(delta.entries.size(), 0u);
    EXPECT_EQ(delta.worldgenVersion, 0xABCDu);
}

TEST_F(FchkCodecTest, DeltaPartialDiffs) {
    auto reference = makeUniformChunk(simulation::material_ids::STONE);
    auto current = reference;

    std::mt19937 rng(123);
    size_t modCount = K_CELL_COUNT / 20;
    std::vector<size_t> indices(K_CELL_COUNT);
    std::iota(indices.begin(), indices.end(), 0);
    std::shuffle(indices.begin(), indices.end(), rng);

    for (size_t i = 0; i < modCount; ++i) {
        size_t base = indices[i] * 4;
        current[base + 0] = static_cast<uint8_t>(simulation::material_ids::SAND);
        current[base + 1] = 130;
        current[base + 2] = 2;
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
    auto reference = makeUniformChunk(simulation::material_ids::STONE);
    auto current = makeRandomChunk(77);

    auto blob = FchkCodec::encodeDelta(current.data(), reference.data(), current.size(), 99, 0);
    auto delta = FchkCodec::decodeDelta(blob);
    EXPECT_EQ(delta.entries.size(), K_CELL_COUNT);

    auto applied = applyDelta(expectedCells(reference), delta);
    auto expected = expectedCells(current);
    EXPECT_EQ(applied, expected);
}

TEST_F(FchkCodecTest, DeltaZstdCompression) {
    auto reference = makeUniformChunk(simulation::material_ids::STONE);
    auto current = reference;
    current[0] = static_cast<uint8_t>(simulation::material_ids::SAND);
    current[1] = 130;
    current[2] = 2;

    auto blob = FchkCodec::encodeDelta(current.data(), reference.data(), current.size(), 1, 1);
    auto delta = FchkCodec::decodeDelta(blob);
    EXPECT_EQ(delta.entries.size(), 1u);

    auto applied = applyDelta(expectedCells(reference), delta);
    auto expected = expectedCells(current);
    EXPECT_EQ(applied, expected);
}

TEST_F(FchkCodecTest, DeltaLz4Compression) {
    auto reference = makeUniformChunk(simulation::material_ids::STONE);
    auto current = reference;
    current[0] = static_cast<uint8_t>(simulation::material_ids::SAND);
    current[1] = 130;
    current[2] = 2;

    auto blob = FchkCodec::encodeDelta(current.data(), reference.data(), current.size(), 1, 2);
    auto delta = FchkCodec::decodeDelta(blob);
    EXPECT_EQ(delta.entries.size(), 1u);

    auto applied = applyDelta(expectedCells(reference), delta);
    auto expected = expectedCells(current);
    EXPECT_EQ(applied, expected);
}

TEST_F(FchkCodecTest, DeltaWithPalette) {
    auto reference = makeUniformChunk(simulation::material_ids::STONE);
    auto current = reference;
    current[0] = static_cast<uint8_t>(simulation::material_ids::SAND);
    current[1] = 130;
    current[2] = 2;

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
    auto cells = makeUniformChunk(simulation::material_ids::STONE);
    auto deltaBlob = FchkCodec::encodeDelta(cells.data(), cells.data(), cells.size(), 0, 1);
    auto fullBlob = FchkCodec::encode(cells.data(), cells.size(), 1);

    EXPECT_TRUE(FchkCodec::isDelta(deltaBlob));
    EXPECT_FALSE(FchkCodec::isDelta(fullBlob));
}

TEST_F(FchkCodecTest, DeltaDecodeRejectsV1) {
    auto cells = makeUniformChunk(simulation::material_ids::STONE);
    auto deltaBlob = FchkCodec::encodeDelta(cells.data(), cells.data(), cells.size(), 0, 0);
    // decode() should reject v2 blobs
    EXPECT_THROW(FchkCodec::decode(deltaBlob), fabric::FabricException);
}

TEST_F(FchkCodecTest, DeltaWorldgenVersion) {
    auto cells = makeUniformChunk(simulation::material_ids::STONE);
    constexpr uint32_t version = 0xDEADBEEF;
    auto blob = FchkCodec::encodeDelta(cells.data(), cells.data(), cells.size(), version, 1);
    auto delta = FchkCodec::decodeDelta(blob);
    EXPECT_EQ(delta.worldgenVersion, version);
}

TEST_F(FchkCodecTest, DeltaSmallerThanFull) {
    auto reference = makeUniformChunk(simulation::material_ids::STONE);
    auto current = reference;

    std::mt19937 rng(456);
    size_t modCount = K_CELL_COUNT / 20;
    std::vector<size_t> indices(K_CELL_COUNT);
    std::iota(indices.begin(), indices.end(), 0);
    std::shuffle(indices.begin(), indices.end(), rng);

    for (size_t i = 0; i < modCount; ++i) {
        size_t base = indices[i] * 4;
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

TEST_F(FchkCodecTest, DecodeAnyHandlesV2Delta) {
    auto reference = makeUniformChunk(simulation::material_ids::STONE);
    auto current = reference;
    current[0] = static_cast<uint8_t>(simulation::material_ids::SAND);
    current[1] = 130;
    current[2] = 2;

    auto blob = FchkCodec::encodeDelta(current.data(), reference.data(), current.size(), 1, 1);
    auto decoded = FchkCodec::decodeAny(blob, reference.data());

    auto expected = expectedCells(current);
    ASSERT_EQ(decoded.cells.size(), expected.size());
    EXPECT_TRUE(std::equal(decoded.cells.begin(), decoded.cells.end(), expected.begin()));
}

TEST_F(FchkCodecTest, DecodeAnyDeltaRequiresRefCells) {
    auto cells = makeUniformChunk(simulation::material_ids::STONE);
    auto blob = FchkCodec::encodeDelta(cells.data(), cells.data(), cells.size(), 0, 0);
    EXPECT_THROW(FchkCodec::decodeAny(blob), fabric::FabricException);
}

TEST_F(FchkCodecTest, HeaderVersionIsV1) {
    auto cells = makeEmptyChunk();
    auto blob = FchkCodec::encode(cells.data(), cells.size());
    FchkHeader header;
    std::memcpy(&header, blob.data_ptr(), sizeof(FchkHeader));
    EXPECT_EQ(header.version, 1);
}

TEST_F(FchkCodecTest, DeltaHeaderVersionIsV2) {
    auto cells = makeEmptyChunk();
    auto blob = FchkCodec::encodeDelta(cells.data(), cells.data(), cells.size(), 0, 0);
    FchkHeader header;
    std::memcpy(&header, blob.data_ptr(), sizeof(FchkHeader));
    EXPECT_EQ(header.version, 2);
}
