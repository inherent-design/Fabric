#include "recurse/persistence/FchkCodec.hh"

#include "fabric/utils/ErrorHandling.hh"
#include "recurse/simulation/VoxelMaterial.hh"
#include <bit>
#include <cstring>
#include <lz4.h>
#include <zstd.h>

static_assert(std::endian::native == std::endian::little, "FchkCodec assumes little-endian byte order");

namespace recurse {

namespace {

/// Convert a single v1/v2 cell (old layout) to v4 cell (new layout) in-place.
/// Old layout (LE): [matId_lo, matId_hi, essenceIdx, flags]
/// New layout (LE): [essenceIdx, displacementRank, phaseAndFlags, spare]
/// Since all materialIds < 256, matId_lo IS the materialId.
void convertV2CellToV4(uint8_t* cell) {
    uint8_t matIdLo = cell[0];
    // cell[1] is matId_hi (always 0)
    uint8_t oldEssenceIdx = cell[2];
    // cell[3] is flags (already stripped of runtime bits)

    // Derive phase and density from materialId using known constants
    using namespace simulation;
    uint8_t phase = 0; // Phase::Empty
    uint8_t density = 0;
    switch (matIdLo) {
        case material_ids::AIR:
            phase = 0;
            density = 0;
            break;
        case material_ids::STONE:
            phase = 1;
            density = 200;
            break; // Phase::Solid
        case material_ids::DIRT:
            phase = 1;
            density = 150;
            break; // Phase::Solid
        case material_ids::SAND:
            phase = 2;
            density = 130;
            break; // Phase::Powder
        case material_ids::WATER:
            phase = 3;
            density = 100;
            break; // Phase::Liquid
        case material_ids::GRAVEL:
            phase = 2;
            density = 170;
            break; // Phase::Powder
        default:
            phase = 1;
            density = 128;
            break; // Unknown: default Solid
    }

    // Write new layout
    cell[0] = matIdLo; // essenceIdx == materialId during migration
    cell[1] = density; // displacementRank
    cell[2] = phase;   // phaseAndFlags (phase in low 3 bits, flags 0)
    cell[3] = 0;       // spare

    // Preserve old essenceIdx: overwrite byte 0 only if old essenceIdx was
    // palette-assigned (non-zero and from a v2 file). For v1 files, old byte 2
    // was temperature (already zeroed by v1 fixup before this function is called
    // for v2 only). For v2, the essenceIdx was a palette index separate from
    // materialId; during migration essenceIdx == materialId, so use matIdLo.
    // The old essenceIdx (palette-based) is discarded because the palette
    // indices in old files are not meaningful in the new layout.
    (void)oldEssenceIdx;
}

} // namespace

ChunkBlob FchkCodec::encode(const void* cells, size_t cellsByteCount, uint8_t compression, int level,
                            const float* paletteData, uint16_t paletteEntryCount) {
    FchkHeader header;
    header.compression = compression;

    const size_t paletteByteCount = static_cast<size_t>(paletteEntryCount) * 4 * sizeof(float);
    const size_t postHeaderSize = cellsByteCount + sizeof(uint16_t) + paletteByteCount;

    // Build uncompressed post-header region: voxel payload + paletteCount + palette entries
    ChunkBlob postHeader(postHeaderSize);
    std::memcpy(postHeader.data_ptr(), cells, cellsByteCount);
    std::memcpy(postHeader.data_ptr() + cellsByteCount, &paletteEntryCount, sizeof(uint16_t));
    if (paletteByteCount > 0) {
        std::memcpy(postHeader.data_ptr() + cellsByteCount + sizeof(uint16_t), paletteData, paletteByteCount);
    }

    if (compression == 0) {
        ChunkBlob blob(sizeof(FchkHeader) + postHeaderSize);
        std::memcpy(blob.data_ptr(), &header, sizeof(FchkHeader));
        std::memcpy(blob.data_ptr() + sizeof(FchkHeader), postHeader.data_ptr(), postHeaderSize);
        return blob;
    }

    if (compression == 1) {
        size_t bound = ZSTD_compressBound(postHeaderSize);
        ChunkBlob blob(sizeof(FchkHeader) + bound);
        std::memcpy(blob.data_ptr(), &header, sizeof(FchkHeader));

        size_t compressedSize =
            ZSTD_compress(blob.data_ptr() + sizeof(FchkHeader), bound, postHeader.data_ptr(), postHeaderSize, level);

        if (ZSTD_isError(compressedSize)) {
            fabric::throwError("zstd compression failed: " + std::string(ZSTD_getErrorName(compressedSize)));
        }

        blob.resize(sizeof(FchkHeader) + compressedSize);
        return blob;
    }

    if (compression == 2) {
        int srcSize = static_cast<int>(postHeaderSize);
        int bound = LZ4_compressBound(srcSize);
        ChunkBlob blob(sizeof(FchkHeader) + static_cast<size_t>(bound));
        std::memcpy(blob.data_ptr(), &header, sizeof(FchkHeader));

        int compressedSize =
            LZ4_compress_default(reinterpret_cast<const char*>(postHeader.data_ptr()),
                                 reinterpret_cast<char*>(blob.data_ptr() + sizeof(FchkHeader)), srcSize, bound);

        if (compressedSize <= 0) {
            fabric::throwError("LZ4 compression failed");
        }

        blob.resize(sizeof(FchkHeader) + static_cast<size_t>(compressedSize));
        return blob;
    }

    fabric::throwError("Unsupported FCHK compression type: " + std::to_string(compression));
}

FchkDecoded FchkCodec::decode(const ChunkBlob& blob) {
    if (blob.size() < sizeof(FchkHeader)) {
        fabric::throwError("FCHK blob too small: " + std::to_string(blob.size()) + " bytes");
    }

    FchkHeader header;
    std::memcpy(&header, blob.data_ptr(), sizeof(FchkHeader));

    if (header.magic[0] != 'F' || header.magic[1] != 'C' || header.magic[2] != 'H' || header.magic[3] != 'K') {
        fabric::throwError("FCHK invalid magic");
    }
    if (header.version == 3) {
        fabric::throwError("FCHK v3 is a delta format; use FchkCodec::decodeDelta()");
    }
    if (header.version < 1 || header.version > 4) {
        fabric::throwError("FCHK unsupported version: " + std::to_string(header.version));
    }

    const uint8_t* payload = blob.data_ptr() + sizeof(FchkHeader);
    size_t payloadSize = blob.size() - sizeof(FchkHeader);

    // Decompress post-header region if needed
    ChunkBlob decompressed;
    const uint8_t* postHeader = nullptr;
    size_t postHeaderSize = 0;

    if (header.compression == 0) {
        postHeader = payload;
        postHeaderSize = payloadSize;
    } else if (header.compression == 1) {
        unsigned long long decompSize = ZSTD_getFrameContentSize(payload, payloadSize);
        if (decompSize == ZSTD_CONTENTSIZE_UNKNOWN || decompSize == ZSTD_CONTENTSIZE_ERROR) {
            fabric::throwError("FCHK zstd: cannot determine decompressed size");
        }

        decompressed.resize(static_cast<size_t>(decompSize));
        size_t actual = ZSTD_decompress(decompressed.data_ptr(), decompressed.size(), payload, payloadSize);
        if (ZSTD_isError(actual)) {
            fabric::throwError("zstd decompression failed: " + std::string(ZSTD_getErrorName(actual)));
        }

        decompressed.resize(actual);
        postHeader = decompressed.data_ptr();
        postHeaderSize = decompressed.size();
    } else if (header.compression == 2) {
        // LZ4: decompressed size derived from chunk dimensions
        size_t expectedCells = static_cast<size_t>(header.dimX) * header.dimY * header.dimZ * 4;
        // Upper bound: cells + palette count (2) + max palette (65535 * 16)
        size_t maxDecomp = expectedCells + sizeof(uint16_t) + 65535 * 16;
        decompressed.resize(maxDecomp);

        int actual = LZ4_decompress_safe(reinterpret_cast<const char*>(payload),
                                         reinterpret_cast<char*>(decompressed.data_ptr()),
                                         static_cast<int>(payloadSize), static_cast<int>(maxDecomp));

        if (actual < 0) {
            fabric::throwError("LZ4 decompression failed");
        }

        decompressed.resize(static_cast<size_t>(actual));
        postHeader = decompressed.data_ptr();
        postHeaderSize = decompressed.size();
    } else {
        fabric::throwError("Unsupported FCHK compression: " + std::to_string(header.compression));
    }

    const size_t cellsByteCount = static_cast<size_t>(header.dimX) * header.dimY * header.dimZ * 4;

    if (postHeaderSize < cellsByteCount) {
        fabric::throwError("FCHK payload too small for dimensions");
    }

    FchkDecoded result;
    result.cells.assign(postHeader, postHeader + cellsByteCount);

    if (header.version == 4) {
        // v4: runtime flags in byte 2 (phaseAndFlags), bits 3-4
        constexpr uint8_t kRuntimeFlagsMask = static_cast<uint8_t>(~(0x08 | 0x10));
        for (size_t i = 2; i < result.cells.size(); i += 4)
            result.cells[i] &= kRuntimeFlagsMask;
    } else if (header.version <= 2) {
        // v1/v2: strip runtime flags from old layout byte 3
        constexpr uint8_t kRuntimeFlagsMask = static_cast<uint8_t>(~(0x01 | 0x02));
        for (size_t i = 3; i < result.cells.size(); i += 4)
            result.cells[i] &= kRuntimeFlagsMask;

        if (header.version == 1) {
            // v1 fixup: zero out old essenceIdx byte (offset 2) before conversion.
            for (size_t i = 2; i < result.cells.size(); i += 4)
                result.cells[i] = 0;
        }

        // Convert old v1/v2 layout to v4 layout in-place
        for (size_t i = 0; i < result.cells.size(); i += 4)
            convertV2CellToV4(&result.cells[i]);

        // v1: no palette section, return early
        if (header.version == 1)
            return result;
    }

    // v2/v4: parse palette section after voxel payload
    const size_t paletteSectionOffset = cellsByteCount;
    if (postHeaderSize >= paletteSectionOffset + sizeof(uint16_t)) {
        std::memcpy(&result.paletteEntryCount, postHeader + paletteSectionOffset, sizeof(uint16_t));

        const size_t paletteByteCount = static_cast<size_t>(result.paletteEntryCount) * 4 * sizeof(float);
        const size_t expectedSize = paletteSectionOffset + sizeof(uint16_t) + paletteByteCount;

        if (postHeaderSize < expectedSize) {
            fabric::throwError("FCHK palette section truncated (v" + std::to_string(header.version) + ")");
        }

        if (result.paletteEntryCount > 0) {
            result.paletteData.resize(static_cast<size_t>(result.paletteEntryCount) * 4);
            std::memcpy(result.paletteData.data(), postHeader + paletteSectionOffset + sizeof(uint16_t),
                        paletteByteCount);
        }
    }

    return result;
}

// --- v3 delta format ---

static constexpr size_t K_CHUNK_VOLUME = 32 * 32 * 32;

ChunkBlob FchkCodec::encodeDelta(const void* currentCells, const void* referenceCells, size_t cellsByteCount,
                                 uint32_t worldgenVersion, uint8_t compression, int level, const float* paletteData,
                                 uint16_t paletteEntryCount) {
    const size_t cellCount = cellsByteCount / 4;
    const auto* cur = static_cast<const uint32_t*>(currentCells);
    const auto* ref = static_cast<const uint32_t*>(referenceCells);

    // Strip runtime-only flags (UPDATED, FREE_FALL) before diffing.
    // These bits are set by the simulation each tick but are not persistent
    // state. Without masking, every "touched" cell produces a false diff
    // against the clean worldgen reference. The decode side already strips
    // these via kRuntimeFlagsMask.
    // New VoxelCell layout: phaseAndFlags in byte 2 (LE). Runtime flags (UPDATED, FREE_FALL)
    // occupy bits 3-4 of that byte (bits 19-20 of the uint32_t). Clear them before diffing.
    constexpr uint32_t kPersistMask = 0xFF'E7'FF'FF;

    std::vector<FchkDeltaEntry> diffs;
    for (size_t i = 0; i < cellCount; ++i) {
        uint32_t c = cur[i] & kPersistMask;
        uint32_t r = ref[i] & kPersistMask;
        if (c != r) {
            diffs.push_back({static_cast<uint32_t>(i), c});
        }
    }

    const auto diffCount = static_cast<uint32_t>(diffs.size());
    const size_t paletteByteCount = static_cast<size_t>(paletteEntryCount) * 4 * sizeof(float);
    const size_t postHeaderSize = sizeof(uint32_t)                                           // worldgenVersion
                                  + sizeof(uint32_t)                                         // diffCount
                                  + diffs.size() * sizeof(FchkDeltaEntry) + sizeof(uint16_t) // paletteEntryCount
                                  + paletteByteCount;

    ChunkBlob postHeader(postHeaderSize);
    uint8_t* dst = postHeader.data_ptr();

    std::memcpy(dst, &worldgenVersion, sizeof(uint32_t));
    dst += sizeof(uint32_t);

    std::memcpy(dst, &diffCount, sizeof(uint32_t));
    dst += sizeof(uint32_t);

    if (!diffs.empty()) {
        std::memcpy(dst, diffs.data(), diffs.size() * sizeof(FchkDeltaEntry));
        dst += diffs.size() * sizeof(FchkDeltaEntry);
    }

    std::memcpy(dst, &paletteEntryCount, sizeof(uint16_t));
    dst += sizeof(uint16_t);

    if (paletteByteCount > 0) {
        std::memcpy(dst, paletteData, paletteByteCount);
    }

    FchkHeader header;
    header.version = 3;
    header.compression = compression;

    if (compression == 0) {
        ChunkBlob blob(sizeof(FchkHeader) + postHeaderSize);
        std::memcpy(blob.data_ptr(), &header, sizeof(FchkHeader));
        std::memcpy(blob.data_ptr() + sizeof(FchkHeader), postHeader.data_ptr(), postHeaderSize);
        return blob;
    }

    if (compression == 1) {
        size_t bound = ZSTD_compressBound(postHeaderSize);
        ChunkBlob blob(sizeof(FchkHeader) + bound);
        std::memcpy(blob.data_ptr(), &header, sizeof(FchkHeader));

        size_t compressedSize =
            ZSTD_compress(blob.data_ptr() + sizeof(FchkHeader), bound, postHeader.data_ptr(), postHeaderSize, level);

        if (ZSTD_isError(compressedSize)) {
            fabric::throwError("zstd compression failed: " + std::string(ZSTD_getErrorName(compressedSize)));
        }

        blob.resize(sizeof(FchkHeader) + compressedSize);
        return blob;
    }

    if (compression == 2) {
        int srcSize = static_cast<int>(postHeaderSize);
        int bound = LZ4_compressBound(srcSize);
        ChunkBlob blob(sizeof(FchkHeader) + static_cast<size_t>(bound));
        std::memcpy(blob.data_ptr(), &header, sizeof(FchkHeader));

        int compressedSize =
            LZ4_compress_default(reinterpret_cast<const char*>(postHeader.data_ptr()),
                                 reinterpret_cast<char*>(blob.data_ptr() + sizeof(FchkHeader)), srcSize, bound);

        if (compressedSize <= 0) {
            fabric::throwError("LZ4 compression failed");
        }

        blob.resize(sizeof(FchkHeader) + static_cast<size_t>(compressedSize));
        return blob;
    }

    fabric::throwError("Unsupported FCHK compression type: " + std::to_string(compression));
}

FchkDeltaDecoded FchkCodec::decodeDelta(const ChunkBlob& blob) {
    if (blob.size() < sizeof(FchkHeader)) {
        fabric::throwError("FCHK blob too small: " + std::to_string(blob.size()) + " bytes");
    }

    FchkHeader header;
    std::memcpy(&header, blob.data_ptr(), sizeof(FchkHeader));

    if (header.magic[0] != 'F' || header.magic[1] != 'C' || header.magic[2] != 'H' || header.magic[3] != 'K') {
        fabric::throwError("FCHK invalid magic");
    }
    if (header.version != 3) {
        fabric::throwError("FCHK decodeDelta requires v3, got v" + std::to_string(header.version));
    }

    const uint8_t* payload = blob.data_ptr() + sizeof(FchkHeader);
    size_t payloadSize = blob.size() - sizeof(FchkHeader);

    // Decompress post-header region if needed
    ChunkBlob decompressed;
    const uint8_t* postHeader = nullptr;
    size_t postHeaderSize = 0;

    if (header.compression == 0) {
        postHeader = payload;
        postHeaderSize = payloadSize;
    } else if (header.compression == 1) {
        unsigned long long decompSize = ZSTD_getFrameContentSize(payload, payloadSize);
        if (decompSize == ZSTD_CONTENTSIZE_UNKNOWN || decompSize == ZSTD_CONTENTSIZE_ERROR) {
            fabric::throwError("FCHK zstd: cannot determine decompressed size");
        }

        decompressed.resize(static_cast<size_t>(decompSize));
        size_t actual = ZSTD_decompress(decompressed.data_ptr(), decompressed.size(), payload, payloadSize);
        if (ZSTD_isError(actual)) {
            fabric::throwError("zstd decompression failed: " + std::string(ZSTD_getErrorName(actual)));
        }

        decompressed.resize(actual);
        postHeader = decompressed.data_ptr();
        postHeaderSize = decompressed.size();
    } else if (header.compression == 2) {
        // Upper bound for v3: worldgen(4) + count(4) + max entries (32768*8) + palette count(2) + max palette
        size_t maxDecomp = 4 + 4 + K_CHUNK_VOLUME * 8 + sizeof(uint16_t) + 65535 * 16;
        decompressed.resize(maxDecomp);

        int actual = LZ4_decompress_safe(reinterpret_cast<const char*>(payload),
                                         reinterpret_cast<char*>(decompressed.data_ptr()),
                                         static_cast<int>(payloadSize), static_cast<int>(maxDecomp));

        if (actual < 0) {
            fabric::throwError("LZ4 decompression failed");
        }

        decompressed.resize(static_cast<size_t>(actual));
        postHeader = decompressed.data_ptr();
        postHeaderSize = decompressed.size();
    } else {
        fabric::throwError("Unsupported FCHK compression: " + std::to_string(header.compression));
    }

    // Parse post-header: worldgenVersion(4) + diffCount(4) + entries(N*8) + paletteCount(2) + palette
    size_t cursor = 0;

    if (postHeaderSize < cursor + sizeof(uint32_t)) {
        fabric::throwError("FCHK v3 payload truncated (worldgenVersion)");
    }
    FchkDeltaDecoded result;
    std::memcpy(&result.worldgenVersion, postHeader + cursor, sizeof(uint32_t));
    cursor += sizeof(uint32_t);

    if (postHeaderSize < cursor + sizeof(uint32_t)) {
        fabric::throwError("FCHK v3 payload truncated (diffCount)");
    }
    uint32_t diffCount = 0;
    std::memcpy(&diffCount, postHeader + cursor, sizeof(uint32_t));
    cursor += sizeof(uint32_t);

    const size_t entriesBytes = static_cast<size_t>(diffCount) * sizeof(FchkDeltaEntry);
    if (postHeaderSize < cursor + entriesBytes) {
        fabric::throwError("FCHK v3 payload truncated (diff entries)");
    }

    result.entries.resize(diffCount);
    if (diffCount > 0) {
        std::memcpy(result.entries.data(), postHeader + cursor, entriesBytes);
    }
    cursor += entriesBytes;

    // Apply runtime flags mask to cell data (v4 layout: phaseAndFlags in byte 2).
    // Note: All v3 delta files in existence use the v4 (MatterState) cell layout
    // because v3 delta format was introduced alongside the MatterState migration.
    // No pre-MatterState v3 deltas exist. The v4 runtime flags mask is correct
    // for all real v3 files. This version branching will be eliminated by the
    // persistence format collapse (feat/persistence-collapse).
    constexpr uint8_t kRuntimeFlagsMask = static_cast<uint8_t>(~(0x08 | 0x10));
    for (auto& e : result.entries) {
        auto* bytes = reinterpret_cast<uint8_t*>(&e.cellData);
        bytes[2] &= kRuntimeFlagsMask;
    }

    // Parse palette section
    if (postHeaderSize < cursor + sizeof(uint16_t)) {
        fabric::throwError("FCHK v3 payload truncated (paletteCount)");
    }
    std::memcpy(&result.paletteEntryCount, postHeader + cursor, sizeof(uint16_t));
    cursor += sizeof(uint16_t);

    const size_t paletteByteCount = static_cast<size_t>(result.paletteEntryCount) * 4 * sizeof(float);
    if (postHeaderSize < cursor + paletteByteCount) {
        fabric::throwError("FCHK v3 palette section truncated");
    }

    if (result.paletteEntryCount > 0) {
        result.paletteData.resize(static_cast<size_t>(result.paletteEntryCount) * 4);
        std::memcpy(result.paletteData.data(), postHeader + cursor, paletteByteCount);
    }

    return result;
}

FchkDecoded FchkCodec::decodeAny(const ChunkBlob& blob, const void* refCells) {
    if (!isDelta(blob))
        return decode(blob);

    if (!refCells)
        fabric::throwError("FCHK v3 delta requires reference cells for decoding");

    auto delta = decodeDelta(blob);

    constexpr size_t cellsByteCount = K_CHUNK_VOLUME * 4;
    FchkDecoded result;
    result.cells.resize(cellsByteCount);
    std::memcpy(result.cells.data(), refCells, cellsByteCount);

    // Clear runtime flags on reference cells (v4 layout: phaseAndFlags in byte 2)
    constexpr uint8_t kRuntimeFlagsMask = static_cast<uint8_t>(~(0x08 | 0x10));
    for (size_t i = 2; i < result.cells.size(); i += 4)
        result.cells[i] &= kRuntimeFlagsMask;

    // Apply delta entries (already have runtime flags cleared from decodeDelta)
    for (const auto& e : delta.entries) {
        size_t offset = static_cast<size_t>(e.cellIndex) * 4;
        std::memcpy(&result.cells[offset], &e.cellData, 4);
    }

    result.paletteData = std::move(delta.paletteData);
    result.paletteEntryCount = delta.paletteEntryCount;
    return result;
}

bool FchkCodec::isDelta(const ChunkBlob& blob) {
    if (blob.size() < sizeof(FchkHeader)) {
        return false;
    }
    FchkHeader header;
    std::memcpy(&header, blob.data_ptr(), sizeof(FchkHeader));
    return header.magic[0] == 'F' && header.magic[1] == 'C' && header.magic[2] == 'H' && header.magic[3] == 'K' &&
           header.version == 3;
}

} // namespace recurse
