#include "recurse/persistence/FilesystemChunkStore.hh"

#include "fabric/utils/ErrorHandling.hh"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace recurse {

FilesystemChunkStore::FilesystemChunkStore(const std::string& worldDir) : chunkDir_(worldDir + "/chunks/gen") {
    fs::create_directories(chunkDir_);
}

// --- v2 API ---

bool FilesystemChunkStore::hasChunk(int cx, int cy, int cz) const {
    return fs::exists(chunkPath(cx, cy, cz));
}

std::optional<ChunkBlob> FilesystemChunkStore::loadChunk(int cx, int cy, int cz) const {
    return loadFile(chunkPath(cx, cy, cz));
}

void FilesystemChunkStore::saveChunk(int cx, int cy, int cz, const ChunkBlob& data) {
    if (data.empty())
        return;
    saveFile(chunkPath(cx, cy, cz), data);
}

size_t FilesystemChunkStore::chunkSize(int cx, int cy, int cz) const {
    return fileSize(chunkPath(cx, cy, cz));
}

// --- Internal helpers ---

std::string FilesystemChunkStore::chunkPath(int cx, int cy, int cz) const {
    return chunkDir_ + "/" + std::to_string(cx) + "_" + std::to_string(cy) + "_" + std::to_string(cz) + ".fchk";
}

std::optional<ChunkBlob> FilesystemChunkStore::loadFile(const std::string& path) const {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in.is_open())
        return std::nullopt;

    auto size = static_cast<size_t>(in.tellg());
    in.seekg(0);

    ChunkBlob blob(size);
    in.read(reinterpret_cast<char*>(blob.data.data()), static_cast<std::streamsize>(size));
    if (!in.good())
        return std::nullopt;

    return blob;
}

void FilesystemChunkStore::saveFile(const std::string& path, const ChunkBlob& data) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        fabric::throwError("Failed to open chunk file for writing: " + path);
    }
    out.write(reinterpret_cast<const char*>(data.data.data()), static_cast<std::streamsize>(data.size()));
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

// --- Batch operations (sorted for sequential I/O) ---

std::vector<std::pair<fabric::ChunkCoord, ChunkBlob>>
FilesystemChunkStore::loadBatch(const std::vector<fabric::ChunkCoord>& coords) const {
    auto sorted = coords;
    std::sort(sorted.begin(), sorted.end(), [](const fabric::ChunkCoord& a, const fabric::ChunkCoord& b) {
        return std::tie(a.x, a.y, a.z) < std::tie(b.x, b.y, b.z);
    });

    std::vector<std::pair<fabric::ChunkCoord, ChunkBlob>> results;
    results.reserve(sorted.size());

    for (const auto& coord : sorted) {
        auto blob = loadChunk(coord.x, coord.y, coord.z);
        if (blob)
            results.push_back({coord, std::move(*blob)});
    }
    return results;
}

void FilesystemChunkStore::saveBatch(const std::vector<std::pair<fabric::ChunkCoord, ChunkBlob>>& entries) {
    std::vector<const std::pair<fabric::ChunkCoord, ChunkBlob>*> sorted;
    sorted.reserve(entries.size());
    for (const auto& e : entries)
        sorted.push_back(&e);
    std::sort(sorted.begin(), sorted.end(), [](const auto* a, const auto* b) {
        return std::tie(a->first.x, a->first.y, a->first.z) < std::tie(b->first.x, b->first.y, b->first.z);
    });

    for (const auto* e : sorted) {
        saveChunk(e->first.x, e->first.y, e->first.z, e->second);
    }
}

} // namespace recurse
