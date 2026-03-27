#include "recurse/world/EssencePalette.hh"

#include "fabric/log/Log.hh"
#include "fabric/utils/ErrorHandling.hh"
#include "fabric/utils/Profiler.hh"
#include <limits>
#include <queue>

using namespace fabric;

namespace recurse {

namespace {

inline float distSq4(const Vector4<float, Space::World>& a, const Vector4<float, Space::World>& b) {
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z, dw = a.w - b.w;
    return dx * dx + dy * dy + dz * dz + dw * dw;
}

} // namespace

EssencePalette::EssencePalette(float epsilon, uint16_t maxSize) : epsilon_(epsilon), maxSize_(maxSize) {}

uint16_t EssencePalette::quantize(const Vector4<float, Space::World>& essence) {
    return addEntry(essence);
}

uint8_t EssencePalette::quantize8(const Vector4<float, Space::World>& essence) {
    uint16_t idx = quantize(essence);
    assert(idx < 256 && "quantize8: palette index exceeds uint8_t range");
    return static_cast<uint8_t>(idx);
}

Vector4<float, Space::World> EssencePalette::lookup(uint16_t index) const {
    if (index >= entries_.size()) {
        throwError("EssencePalette::lookup: index out of range");
    }
    return entries_[index];
}

uint16_t EssencePalette::addEntry(const Vector4<float, Space::World>& essence) {
    float epsSq = epsilon_ * epsilon_;

    if (epsilon_ > 0.0f) {
        // O(1) grid hash lookup. The vector stores all indices that hash to
        // the same grid key; iterate to find an entry within epsilon distance.
        auto key = toGridKey(essence);
        auto it = gridMap_.find(key);
        if (it != gridMap_.end()) {
            for (uint16_t idx : it->second) {
                if (distSq4(entries_[idx], essence) <= epsSq) {
                    return idx;
                }
            }
        }

        if (entries_.size() < maxSize_) {
            auto idx = static_cast<uint16_t>(entries_.size());
            entries_.push_back(essence);
            gridMap_[key].push_back(idx);
            return idx;
        }

        // Overflow: batch-merge K closest pairs, rebuild grid map, retry.
        FABRIC_LOG_DEBUG("EssencePalette merge: {} entries at capacity", maxSize_);
        uint16_t mergedCount = mergeBatch(K_BATCH_MERGE_K);
        FABRIC_LOG_TRACE("EssencePalette merge: merged {} pairs, now {} entries", static_cast<int>(mergedCount),
                         entries_.size());
        (void)mergedCount;
        rebuildGridMap();

        it = gridMap_.find(key);
        if (it != gridMap_.end()) {
            for (uint16_t idx : it->second) {
                if (distSq4(entries_[idx], essence) <= epsSq) {
                    return idx;
                }
            }
        }

        auto idx = static_cast<uint16_t>(entries_.size());
        entries_.push_back(essence);
        gridMap_[key].push_back(idx);
        return idx;
    }

    // epsilon == 0: exact-match linear scan (test paths only).
    for (size_t i = 0; i < entries_.size(); ++i) {
        if (distSq4(entries_[i], essence) <= epsSq) {
            return static_cast<uint16_t>(i);
        }
    }

    if (entries_.size() >= maxSize_) {
        mergeClosestPair();
        for (size_t i = 0; i < entries_.size(); ++i) {
            if (distSq4(entries_[i], essence) <= epsSq) {
                return static_cast<uint16_t>(i);
            }
        }
    }

    auto idx = static_cast<uint16_t>(entries_.size());
    entries_.push_back(essence);
    return idx;
}

uint16_t EssencePalette::addEntryRaw(const Vector4<float, Space::World>& essence) {
    auto idx = static_cast<uint16_t>(entries_.size());
    entries_.push_back(essence);
    if (epsilon_ > 0.0f)
        gridMap_[toGridKey(essence)].push_back(idx);
    return idx;
}

uint16_t EssencePalette::mergeClosestPair() {
    FABRIC_ZONE_SCOPED;

    if (entries_.size() < 2)
        return 0;

    size_t bestA = 0;
    size_t bestB = 1;
    float bestDistSq = std::numeric_limits<float>::max();

    for (size_t i = 0; i < entries_.size(); ++i) {
        for (size_t j = i + 1; j < entries_.size(); ++j) {
            float d = distSq4(entries_[i], entries_[j]);
            if (d < bestDistSq) {
                bestDistSq = d;
                bestA = i;
                bestB = j;
            }
        }
    }

    // Merge B into A (midpoint)
    entries_[bestA] = (entries_[bestA] + entries_[bestB]) * 0.5f;

    // Remove B by swapping with the last entry
    if (bestB != entries_.size() - 1)
        entries_[bestB] = entries_.back();
    entries_.pop_back();

    return static_cast<uint16_t>(bestA);
}

uint16_t EssencePalette::mergeBatch(uint16_t k) {
    FABRIC_ZONE_SCOPED;

    if (entries_.size() < 2)
        return 0;

    // Collect up to K closest pairs using a max-heap of size K.
    // O(n^2) brute force; acceptable because palette sizes are small
    // and batch merge reduces call frequency from ~559K to near-zero.
    using Pair = std::tuple<float, size_t, size_t>;
    std::priority_queue<Pair> heap; // max-heap by distance

    for (size_t i = 0; i < entries_.size(); ++i) {
        for (size_t j = i + 1; j < entries_.size(); ++j) {
            float d = distSq4(entries_[i], entries_[j]);
            if (heap.size() < static_cast<size_t>(k)) {
                heap.push({d, i, j});
            } else if (d < std::get<0>(heap.top())) {
                heap.pop();
                heap.push({d, i, j});
            }
        }
    }

    // Extract pairs from heap
    struct MergeOp {
        size_t survivor;
        size_t removed;
        float distSq;
    };
    std::vector<MergeOp> merges;
    merges.reserve(heap.size());

    // Mark which indices are consumed
    std::vector<bool> consumed(entries_.size(), false);

    while (!heap.empty()) {
        auto [d, a, b] = heap.top();
        heap.pop();
        if (consumed[a] || consumed[b])
            continue;
        merges.push_back({a, b, d});
        consumed[b] = true;
    }

    // Apply merges. Process in reverse order of b index to avoid
    // invalidating higher indices during removal.
    std::sort(merges.begin(), merges.end(), [](const MergeOp& x, const MergeOp& y) { return x.removed > y.removed; });

    for (auto& m : merges) {
        if (m.removed >= entries_.size())
            continue;
        entries_[m.survivor] = (entries_[m.survivor] + entries_[m.removed]) * 0.5f;
        if (m.removed != entries_.size() - 1)
            entries_[m.removed] = entries_.back();
        entries_.pop_back();
    }

    return static_cast<uint16_t>(merges.size());
}

EssencePalette::GridKey EssencePalette::toGridKey(const Vector4<float, Space::World>& v) const {
    float inv = 1.0f / epsilon_;
    return {{
        static_cast<int16_t>(std::lroundf(v.x * inv)),
        static_cast<int16_t>(std::lroundf(v.y * inv)),
        static_cast<int16_t>(std::lroundf(v.z * inv)),
        static_cast<int16_t>(std::lroundf(v.w * inv)),
    }};
}

void EssencePalette::rebuildGridMap() {
    gridMap_.clear();
    gridMap_.reserve(entries_.size() * 2);
    for (size_t i = 0; i < entries_.size(); ++i) {
        gridMap_[toGridKey(entries_[i])].push_back(static_cast<uint16_t>(i));
    }
}

size_t EssencePalette::paletteSize() const {
    return entries_.size();
}

uint16_t EssencePalette::maxSize() const {
    return maxSize_;
}

float EssencePalette::epsilon() const {
    return epsilon_;
}

void EssencePalette::setEpsilon(float eps) {
    epsilon_ = eps;
    rebuildGridMap();
}

void EssencePalette::clear() {
    entries_.clear();
    gridMap_.clear();
}

} // namespace recurse
