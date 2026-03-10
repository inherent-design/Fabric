#include "recurse/persistence/FilesystemChunkStore.hh"

#include "fabric/utils/ErrorHandling.hh"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <lz4.h>
#include <zstd.h>

namespace fs = std::filesystem;

namespace recurse {

FilesystemChunkStore::FilesystemChunkStore(const std::string& worldDir)
    : genDir_(worldDir + "/chunks/gen"), deltaDir_(worldDir + "/chunks/delta") {
    fs::create_directories(genDir_);
    fs::create_directories(deltaDir_);
}

// --- Gen data ---

bool FilesystemChunkStore::hasGenData(int cx, int cy, int cz) const {
    return fs::exists(genPath(cx, cy, cz));
}

std::optional<ChunkBlob> FilesystemChunkStore::loadGenData(int cx, int cy, int cz) const {
    return loadFile(genPath(cx, cy, cz));
}

void FilesystemChunkStore::saveGenData(int cx, int cy, int cz, const ChunkBlob& data) {
    saveFile(genPath(cx, cy, cz), data);
}

// --- Delta data ---

bool FilesystemChunkStore::hasDelta(int cx, int cy, int cz) const {
    return fs::exists(deltaPath(cx, cy, cz));
}

std::optional<ChunkBlob> FilesystemChunkStore::loadDelta(int cx, int cy, int cz) const {
    return loadFile(deltaPath(cx, cy, cz));
}

void FilesystemChunkStore::saveDelta(int cx, int cy, int cz, const ChunkBlob& data) {
    saveFile(deltaPath(cx, cy, cz), data);
}

// --- Compaction ---

void FilesystemChunkStore::compactChunk(int cx, int cy, int cz) {
    auto dp = deltaPath(cx, cy, cz);
    if (!fs::exists(dp))
        return;

    auto gp = genPath(cx, cy, cz);
    // Delta becomes the new gen data (full snapshot replacement)
    fs::rename(dp, gp);
}

// --- Size queries ---

size_t FilesystemChunkStore::deltaSize(int cx, int cy, int cz) const {
    return fileSize(deltaPath(cx, cy, cz));
}

size_t FilesystemChunkStore::genDataSize(int cx, int cy, int cz) const {
    return fileSize(genPath(cx, cy, cz));
}

// --- FCHK encode/decode ---

ChunkBlob FilesystemChunkStore::encode(const void* cells, size_t cellsByteCount, uint8_t compression, int level,
                                       const float* paletteData, uint16_t paletteEntryCount) {
    FchkHeader header;
    header.compression = compression;

    const size_t paletteByteCount = static_cast<size_t>(paletteEntryCount) * 4 * sizeof(float);
    const size_t postHeaderSize = cellsByteCount + sizeof(uint16_t) + paletteByteCount;

    // Build uncompressed post-header region: voxel payload + paletteCount + palette entries
    ChunkBlob postHeader(postHeaderSize);
    std::memcpy(postHeader.data(), cells, cellsByteCount);
    std::memcpy(postHeader.data() + cellsByteCount, &paletteEntryCount, sizeof(uint16_t));
    if (paletteByteCount > 0) {
        std::memcpy(postHeader.data() + cellsByteCount + sizeof(uint16_t), paletteData, paletteByteCount);
    }

    if (compression == 0) {
        ChunkBlob blob(sizeof(FchkHeader) + postHeaderSize);
        std::memcpy(blob.data(), &header, sizeof(FchkHeader));
        std::memcpy(blob.data() + sizeof(FchkHeader), postHeader.data(), postHeaderSize);
        return blob;
    }

    if (compression == 1) {
        size_t bound = ZSTD_compressBound(postHeaderSize);
        ChunkBlob blob(sizeof(FchkHeader) + bound);
        std::memcpy(blob.data(), &header, sizeof(FchkHeader));

        size_t compressedSize =
            ZSTD_compress(blob.data() + sizeof(FchkHeader), bound, postHeader.data(), postHeaderSize, level);

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
        std::memcpy(blob.data(), &header, sizeof(FchkHeader));

        int compressedSize =
            LZ4_compress_default(reinterpret_cast<const char*>(postHeader.data()),
                                 reinterpret_cast<char*>(blob.data() + sizeof(FchkHeader)), srcSize, bound);

        if (compressedSize <= 0) {
            fabric::throwError("LZ4 compression failed");
        }

        blob.resize(sizeof(FchkHeader) + static_cast<size_t>(compressedSize));
        return blob;
    }

    fabric::throwError("Unsupported FCHK compression type: " + std::to_string(compression));
}

FchkDecoded FilesystemChunkStore::decode(const ChunkBlob& blob) {
    if (blob.size() < sizeof(FchkHeader)) {
        fabric::throwError("FCHK blob too small: " + std::to_string(blob.size()) + " bytes");
    }

    FchkHeader header;
    std::memcpy(&header, blob.data(), sizeof(FchkHeader));

    if (header.magic[0] != 'F' || header.magic[1] != 'C' || header.magic[2] != 'H' || header.magic[3] != 'K') {
        fabric::throwError("FCHK invalid magic");
    }
    if (header.version < 1 || header.version > 2) {
        fabric::throwError("FCHK unsupported version: " + std::to_string(header.version));
    }

    const uint8_t* payload = blob.data() + sizeof(FchkHeader);
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
        size_t actual = ZSTD_decompress(decompressed.data(), decompressed.size(), payload, payloadSize);
        if (ZSTD_isError(actual)) {
            fabric::throwError("zstd decompression failed: " + std::string(ZSTD_getErrorName(actual)));
        }

        decompressed.resize(actual);
        postHeader = decompressed.data();
        postHeaderSize = decompressed.size();
    } else if (header.compression == 2) {
        // LZ4: decompressed size derived from chunk dimensions
        size_t expectedCells = static_cast<size_t>(header.dimX) * header.dimY * header.dimZ * 4;
        // Upper bound: cells + palette count (2) + max palette (65535 * 16)
        size_t maxDecomp = expectedCells + sizeof(uint16_t) + 65535 * 16;
        decompressed.resize(maxDecomp);

        int actual =
            LZ4_decompress_safe(reinterpret_cast<const char*>(payload), reinterpret_cast<char*>(decompressed.data()),
                                static_cast<int>(payloadSize), static_cast<int>(maxDecomp));

        if (actual < 0) {
            fabric::throwError("LZ4 decompression failed");
        }

        decompressed.resize(static_cast<size_t>(actual));
        postHeader = decompressed.data();
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

// --- Internal helpers ---

std::string FilesystemChunkStore::genPath(int cx, int cy, int cz) const {
    return genDir_ + "/" + std::to_string(cx) + "_" + std::to_string(cy) + "_" + std::to_string(cz) + ".fchk";
}

std::string FilesystemChunkStore::deltaPath(int cx, int cy, int cz) const {
    return deltaDir_ + "/" + std::to_string(cx) + "_" + std::to_string(cy) + "_" + std::to_string(cz) + ".fchk";
}

std::optional<ChunkBlob> FilesystemChunkStore::loadFile(const std::string& path) const {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in.is_open())
        return std::nullopt;

    auto size = static_cast<size_t>(in.tellg());
    in.seekg(0);

    ChunkBlob blob(size);
    in.read(reinterpret_cast<char*>(blob.data()), static_cast<std::streamsize>(size));
    if (!in.good())
        return std::nullopt;

    return blob;
}

void FilesystemChunkStore::saveFile(const std::string& path, const ChunkBlob& data) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        fabric::throwError("Failed to open chunk file for writing: " + path);
    }
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!out.good()) {
        fabric::throwError("Failed to write chunk file: " + path);
    }
}

size_t FilesystemChunkStore::fileSize(const std::string& path) const {
    std::error_code ec;
    auto sz = fs::file_size(path, ec);
    if (ec)
        return 0;
    return static_cast<size_t>(sz);
}

} // namespace recurse
