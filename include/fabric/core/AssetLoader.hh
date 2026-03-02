#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace fabric {

class ResourceHub;

/// Abstract base for loading assets of type T from disk.
/// Implementations are registered with AssetRegistry via registerLoader<T>().
template <typename T> class AssetLoader {
  public:
    virtual ~AssetLoader() = default;

    /// Load an asset from the given path.
    virtual std::unique_ptr<T> load(const std::filesystem::path& path, ResourceHub& hub) = 0;

    /// File extensions this loader handles (e.g., {".test", ".dat"}).
    virtual std::vector<std::string> extensions() const = 0;
};

} // namespace fabric
