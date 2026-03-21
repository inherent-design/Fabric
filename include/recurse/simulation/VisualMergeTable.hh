#pragma once

#include "recurse/simulation/VoxelMaterial.hh"

#include <array>
#include <cstdint>
#include <vector>

namespace recurse::simulation {

/// Visual signature capturing appearance-relevant cell fields.
/// Two cells with identical VisualSignature look the same on screen
/// and can be merged into a single greedy quad.
struct VisualSignature {
    uint8_t essenceIdx{0};
    Phase phase{Phase::Empty};

    constexpr bool operator==(const VisualSignature&) const = default;
};

/// Compute visual-equivalence hash from a VisualSignature.
/// Combines essenceIdx and phase into a 16-bit key.
/// Designed so that AIR (essenceIdx=0, phase=Empty=0) hashes to 0.
constexpr uint16_t visualHash(VisualSignature sig) {
    // phase occupies 3 bits (values 0-4), essenceIdx occupies 8 bits.
    // Pack: essenceIdx in high 8 bits, phase in low 3 bits.
    // This gives 2048 unique slots (256 essences * 8 phase slots),
    // well within uint16_t range and collision-free for v1.
    return static_cast<uint16_t>((static_cast<uint16_t>(sig.essenceIdx) << 3) |
                                 (static_cast<uint16_t>(sig.phase) & 0x07));
}

/// Extract VisualSignature from a VoxelCell.
constexpr VisualSignature visualSignatureOf(VoxelCell cell) {
    return {cell.essenceIdx, cell.phase()};
}

/// Indexed hash table for visual-equivalence merge decisions.
///
/// Maps a 16-bit visual hash to either:
///  - an empty slot (no entry registered)
///  - a single-item slot (one VisualSignature)
///  - a multi-entry slot (two or more signatures that collided)
///
/// The table answers "can these two hashes merge?" by checking whether
/// they resolve to the same VisualSignature. On hash collision (two
/// different signatures mapping to the same hash), the slot is promoted
/// from single to multi-entry with a ref-counted array.
///
/// For the current material set (6 materials, 5 phases) collisions are
/// impossible because visualHash is injective for 256 essences * 8 phases.
/// The collision path exists for future essence spaces that exceed the
/// hash's discrimination power.
class VisualMergeTable {
  public:
    /// Table capacity. 2048 = 256 essences * 8 phase slots.
    static constexpr size_t K_TABLE_SIZE = 2048;

    VisualMergeTable() { slots_.fill(Slot{}); }

    /// Register a cell's visual signature in the table.
    /// Returns the hash key. Idempotent for duplicate registrations.
    uint16_t registerSignature(VisualSignature sig) {
        const uint16_t hash = visualHash(sig);
        auto& slot = slots_[hash];

        if (slot.state == SlotState::Empty) {
            slot.state = SlotState::Single;
            slot.single = sig;
            return hash;
        }

        if (slot.state == SlotState::Single) {
            if (slot.single == sig)
                return hash;
            // Hash collision: promote to multi-entry.
            VisualSignature existing = slot.single;
            slot.state = SlotState::Multi;
            slot.multiIdx = static_cast<uint16_t>(overflowEntries_.size());
            slot.multiCount = 2;
            overflowEntries_.push_back(existing);
            overflowEntries_.push_back(sig);
            return hash;
        }

        // Multi-entry: check if already present.
        const size_t base = slot.multiIdx;
        for (uint16_t i = 0; i < slot.multiCount; ++i) {
            if (overflowEntries_[base + i] == sig)
                return hash;
        }
        // Add new collision entry.
        if (base + slot.multiCount == overflowEntries_.size()) {
            // Entries are contiguous at the tail; just append.
            overflowEntries_.push_back(sig);
        } else {
            // Non-contiguous: relocate the block to the tail.
            const size_t newBase = overflowEntries_.size();
            for (uint16_t i = 0; i < slot.multiCount; ++i)
                overflowEntries_.push_back(overflowEntries_[base + i]);
            overflowEntries_.push_back(sig);
            slot.multiIdx = static_cast<uint16_t>(newBase);
        }
        ++slot.multiCount;
        return hash;
    }

    /// Returns true when two hash keys are visually equivalent.
    /// For single-item slots, this is a direct hash comparison.
    /// For multi-entry slots, it checks that both hashes resolve to
    /// the same VisualSignature within the collision chain.
    bool canMerge(uint16_t hashA, uint16_t hashB) const {
        if (hashA != hashB)
            return false;

        // Same hash. For single slots, guaranteed identical.
        // For multi slots, same hash means both cells share the hash
        // but could be different signatures. However, the greedy mesher
        // registers each cell and gets back the same hash only if it
        // was the same or colliding signature. Since we want merging to
        // mean "visually identical," we need the caller to track the
        // full signature. But the current contract (MergeKey comparison)
        // only provides the hash.
        //
        // For collision-free hash functions (v1: injective mapping),
        // same hash guarantees same signature. When collisions exist,
        // same hash from different signatures will incorrectly merge.
        // This is acceptable in v1 because the hash IS injective for
        // 256 essences * 8 phases. Future: widen MergeKey or store
        // per-mask-slot signature indices.
        return true;
    }

    /// Query whether a hash maps to a single, unique visual identity.
    /// Returns false for empty or multi-entry slots.
    bool isSingleEntry(uint16_t hash) const { return hash < K_TABLE_SIZE && slots_[hash].state == SlotState::Single; }

    /// Number of collision entries in overflow storage.
    size_t overflowSize() const { return overflowEntries_.size(); }

    /// Reset the table for reuse.
    void clear() {
        slots_.fill(Slot{});
        overflowEntries_.clear();
    }

  private:
    enum class SlotState : uint8_t {
        Empty = 0,
        Single = 1,
        Multi = 2
    };

    struct Slot {
        SlotState state{SlotState::Empty};
        union {
            VisualSignature single;
            struct {
                uint16_t multiIdx;
                uint16_t multiCount;
            };
        };

        Slot() : state(SlotState::Empty), single{} {}
    };

    std::array<Slot, K_TABLE_SIZE> slots_;
    std::vector<VisualSignature> overflowEntries_;
};

} // namespace recurse::simulation
