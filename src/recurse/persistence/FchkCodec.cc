#include "recurse/persistence/FchkCodec.hh"

#include "fabric/utils/ErrorHandling.hh"
#include <bit>
#include <cstring>
#include <lz4.h>
#include <zstd.h>

static_assert(std::endian::native == std::endian::little, "FchkCodec assumes little-endian byte order");

namespace recurse {

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
    if (header.version < 1 || header.version > 2) {
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

    if (header.version == 1) {
        // v1 fixup: zero out essenceIdx byte (offset 2 within each 4-byte VoxelCell).
        // Old files have temperature=128 in that byte, which would cause OOB palette lookups.
        for (size_t i = 2; i < result.cells.size(); i += 4) {
            result.cells[i] = 0;
        }
        return result;
    }

    // v2: parse palette section after voxel payload
    const size_t paletteSectionOffset = cellsByteCount;
    if (postHeaderSize >= paletteSectionOffset + sizeof(uint16_t)) {
        std::memcpy(&result.paletteEntryCount, postHeader + paletteSectionOffset, sizeof(uint16_t));

        const size_t paletteByteCount = static_cast<size_t>(result.paletteEntryCount) * 4 * sizeof(float);
        const size_t expectedSize = paletteSectionOffset + sizeof(uint16_t) + paletteByteCount;

        if (postHeaderSize < expectedSize) {
            fabric::throwError("FCHK v2 palette section truncated");
        }

        if (result.paletteEntryCount > 0) {
            result.paletteData.resize(static_cast<size_t>(result.paletteEntryCount) * 4);
            std::memcpy(result.paletteData.data(), postHeader + paletteSectionOffset + sizeof(uint16_t),
                        paletteByteCount);
        }
    }

    return result;
}

} // namespace recurse
