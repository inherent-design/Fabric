#include "recurse/persistence/FilesystemChunkStore.hh"

#include "fabric/utils/ErrorHandling.hh"
#include <cstring>
#include <filesystem>
#include <fstream>

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

ChunkBlob FilesystemChunkStore::encode(const void* cells, size_t byteCount) {
    FchkHeader header;
    ChunkBlob blob(sizeof(FchkHeader) + byteCount);
    std::memcpy(blob.data(), &header, sizeof(FchkHeader));
    std::memcpy(blob.data() + sizeof(FchkHeader), cells, byteCount);
    return blob;
}

std::pair<const uint8_t*, size_t> FilesystemChunkStore::decodeView(const ChunkBlob& blob) {
    if (blob.size() < sizeof(FchkHeader)) {
        fabric::throwError("FCHK blob too small: " + std::to_string(blob.size()) + " bytes");
    }

    FchkHeader header;
    std::memcpy(&header, blob.data(), sizeof(FchkHeader));

    if (header.magic[0] != 'F' || header.magic[1] != 'C' || header.magic[2] != 'H' || header.magic[3] != 'K') {
        fabric::throwError("FCHK invalid magic");
    }
    if (header.version != 1) {
        fabric::throwError("FCHK unsupported version: " + std::to_string(header.version));
    }
    if (header.compression != 0) {
        fabric::throwError("FCHK compression not supported in v1: " + std::to_string(header.compression));
    }

    size_t payloadSize = blob.size() - sizeof(FchkHeader);
    return {blob.data() + sizeof(FchkHeader), payloadSize};
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
