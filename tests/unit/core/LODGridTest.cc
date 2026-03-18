#include "recurse/render/LODGrid.hh"
#include "fabric/platform/JobScheduler.hh"
#include "recurse/render/LODMeshManager.hh"
#include "recurse/simulation/MaterialRegistry.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/VoxelMaterial.hh"
#include "recurse/world/MinecraftNoiseGenerator.hh"
#include "recurse/world/NaturalWorldGenerator.hh"
#include <algorithm>
#include <gtest/gtest.h>

using namespace recurse;
using namespace recurse::simulation;
using recurse::simulation::K_CHUNK_SIZE;
using recurse::simulation::K_CHUNK_VOLUME;

namespace {

void fillSectionFromWorldGen(LODSection& section, WorldGenerator& gen, int cx, int cy, int cz) {
    section.origin = LODGrid::sectionOrigin(0, cx, cy, cz);
    section.palette.clear();
    section.palette.push_back(material_ids::AIR);
    section.blockIndices.assign(LODSection::K_VOLUME, 0);

    for (int lz = 0; lz < LODSection::K_SIZE; ++lz) {
        for (int ly = 0; ly < LODSection::K_SIZE; ++ly) {
            for (int lx = 0; lx < LODSection::K_SIZE; ++lx) {
                int wx = section.origin.x + lx;
                int wy = section.origin.y + ly;
                int wz = section.origin.z + lz;

                uint16_t matId = gen.sampleMaterial(wx, wy, wz);
                uint16_t palIdx = 0;
                auto it = std::find(section.palette.begin(), section.palette.end(), matId);
                if (it != section.palette.end()) {
                    palIdx = static_cast<uint16_t>(std::distance(section.palette.begin(), it));
                } else {
                    palIdx = static_cast<uint16_t>(section.palette.size());
                    section.palette.push_back(matId);
                }
                section.set(lx, ly, lz, palIdx);
            }
        }
    }

    section.dirty = true;
}

void fillSectionFromGrid(LODSection& section, const SimulationGrid& grid, int cx, int cy, int cz) {
    section.origin = LODGrid::sectionOrigin(0, cx, cy, cz);
    section.palette.clear();
    section.palette.push_back(material_ids::AIR);
    section.blockIndices.assign(LODSection::K_VOLUME, 0);

    for (int lz = 0; lz < LODSection::K_SIZE; ++lz) {
        for (int ly = 0; ly < LODSection::K_SIZE; ++ly) {
            for (int lx = 0; lx < LODSection::K_SIZE; ++lx) {
                int wx = section.origin.x + lx;
                int wy = section.origin.y + ly;
                int wz = section.origin.z + lz;

                uint16_t matId = grid.readCell(wx, wy, wz).materialId;
                uint16_t palIdx = 0;
                auto it = std::find(section.palette.begin(), section.palette.end(), matId);
                if (it != section.palette.end()) {
                    palIdx = static_cast<uint16_t>(std::distance(section.palette.begin(), it));
                } else {
                    palIdx = static_cast<uint16_t>(section.palette.size());
                    section.palette.push_back(matId);
                }
                section.set(lx, ly, lz, palIdx);
            }
        }
    }

    section.dirty = true;
}

void fillUniformSection(LODSection& section, uint16_t materialId) {
    section.palette = {material_ids::AIR};
    if (materialId != material_ids::AIR) {
        section.palette.push_back(materialId);
    }
    section.blockIndices.assign(LODSection::K_VOLUME, materialId == material_ids::AIR ? 0 : 1);
    section.dirty = true;
}

uint16_t sectionMaterialAt(const LODSection& section, int lx, int ly, int lz) {
    return section.materialOf(section.get(lx, ly, lz));
}

} // namespace

// -- Parallel LOD fill from WorldGenerator (C-P2) ----------------------------

TEST(LODGrid, ParallelFill_DirectMatchesSequential) {
    recurse::NoiseGenConfig cfg;
    NaturalWorldGenerator gen(cfg);
    fabric::JobScheduler scheduler(2);

    std::tuple<int, int, int> coords[] = {{0, 0, 0}, {1, 0, 0}, {0, -1, 0}, {-1, 0, 1}};

    // Sequential: fill 4 sections one at a time
    LODGrid seqGrid;
    for (auto [cx, cy, cz] : coords) {
        auto* section = seqGrid.getOrCreate(0, cx, cy, cz);
        ASSERT_NE(section, nullptr);
        fillSectionFromWorldGen(*section, gen, cx, cy, cz);
    }

    // Parallel: fill 4 sections via parallelFor
    LODGrid parGrid;
    struct GenTask {
        LODSection* section;
        int cx, cy, cz;
    };
    std::vector<GenTask> tasks;
    for (auto [cx, cy, cz] : coords) {
        auto* section = parGrid.getOrCreate(0, cx, cy, cz);
        ASSERT_NE(section, nullptr);
        tasks.push_back({section, cx, cy, cz});
    }

    scheduler.parallelFor(tasks.size(), [&tasks, &gen](size_t idx, size_t) {
        fillSectionFromWorldGen(*tasks[idx].section, gen, tasks[idx].cx, tasks[idx].cy, tasks[idx].cz);
    });

    for (auto [cx, cy, cz] : coords) {
        auto key = LODSectionKey::make(0, cx, cy, cz);
        auto* seqSection = seqGrid.get(key);
        auto* parSection = parGrid.get(key);
        ASSERT_NE(seqSection, nullptr);
        ASSERT_NE(parSection, nullptr);
        EXPECT_TRUE(parSection->dirty);
        for (int i = 0; i < LODSection::K_VOLUME; ++i) {
            uint16_t seqMat = seqSection->materialOf(seqSection->blockIndices[i]);
            uint16_t parMat = parSection->materialOf(parSection->blockIndices[i]);
            EXPECT_EQ(parMat, seqMat) << "section (" << cx << "," << cy << "," << cz << ") index=" << i;
        }
    }
}

// -- Parallel LOD fill from SimulationGrid (C-P2) ----------------------------

TEST(LODGrid, ParallelFill_GridMatchesSequential) {
    recurse::NoiseGenConfig cfg;
    NaturalWorldGenerator gen(cfg);
    fabric::JobScheduler scheduler(2);

    std::tuple<int, int, int> coords[] = {{0, 0, 0}, {1, 0, 0}, {0, -1, 0}, {-1, 0, 1}};

    SimulationGrid simGrid;
    for (auto [cx, cy, cz] : coords) {
        simGrid.registry().addChunk(cx, cy, cz);
        simGrid.materializeChunk(cx, cy, cz);
        auto* buf = simGrid.writeBuffer(cx, cy, cz);
        ASSERT_NE(buf, nullptr);
        gen.generateToBuffer(buf->data(), cx, cy, cz);
        simGrid.syncChunkBuffers(cx, cy, cz);
    }
    simGrid.advanceEpoch();

    // Sequential LOD fill
    LODGrid seqGrid;
    for (auto [cx, cy, cz] : coords) {
        auto* section = seqGrid.getOrCreate(0, cx, cy, cz);
        ASSERT_NE(section, nullptr);
        fillSectionFromGrid(*section, simGrid, cx, cy, cz);
    }

    // Parallel LOD fill
    LODGrid parGrid;
    struct GenTask {
        LODSection* section;
        int cx, cy, cz;
    };
    std::vector<GenTask> tasks;
    for (auto [cx, cy, cz] : coords) {
        auto* section = parGrid.getOrCreate(0, cx, cy, cz);
        ASSERT_NE(section, nullptr);
        tasks.push_back({section, cx, cy, cz});
    }

    const auto* gridPtr = &simGrid;
    scheduler.parallelFor(tasks.size(), [&tasks, gridPtr](size_t idx, size_t) {
        fillSectionFromGrid(*tasks[idx].section, *gridPtr, tasks[idx].cx, tasks[idx].cy, tasks[idx].cz);
    });

    for (auto [cx, cy, cz] : coords) {
        auto key = LODSectionKey::make(0, cx, cy, cz);
        auto* seqSection = seqGrid.get(key);
        auto* parSection = parGrid.get(key);
        ASSERT_NE(seqSection, nullptr);
        ASSERT_NE(parSection, nullptr);
        EXPECT_TRUE(parSection->dirty);
        for (int i = 0; i < LODSection::K_VOLUME; ++i) {
            uint16_t seqMat = seqSection->materialOf(seqSection->blockIndices[i]);
            uint16_t parMat = parSection->materialOf(parSection->blockIndices[i]);
            EXPECT_EQ(parMat, seqMat) << "section (" << cx << "," << cy << "," << cz << ") index=" << i;
        }
    }
}

// -- Batch dirty flag verification -------------------------------------------

TEST(LODGrid, ParallelFill_AllSectionsMarkedDirty) {
    recurse::NoiseGenConfig cfg;
    NaturalWorldGenerator gen(cfg);
    fabric::JobScheduler scheduler(2);

    LODGrid grid;
    struct GenTask {
        LODSection* section;
        int cx, cy, cz;
    };
    std::vector<GenTask> tasks;
    for (int i = 0; i < 8; ++i) {
        auto* section = grid.getOrCreate(0, i, 0, 0);
        ASSERT_NE(section, nullptr);
        section->dirty = false;
        tasks.push_back({section, i, 0, 0});
    }

    scheduler.parallelFor(tasks.size(), [&tasks, &gen](size_t idx, size_t) {
        fillSectionFromWorldGen(*tasks[idx].section, gen, tasks[idx].cx, tasks[idx].cy, tasks[idx].cz);
    });

    for (int i = 0; i < 8; ++i) {
        auto key = LODSectionKey::make(0, i, 0, 0);
        auto* section = grid.get(key);
        ASSERT_NE(section, nullptr);
        EXPECT_TRUE(section->dirty) << "Section " << i << " should be dirty after parallelFor";
    }
}

// -- Empty sections above terrain --------------------------------------------

TEST(LODGrid, ParallelFill_EmptySectionsAboveTerrain) {
    recurse::NoiseGenConfig cfg;
    cfg.terrainHeight = 16.0f;
    cfg.seaLevel = 8.0f;
    NaturalWorldGenerator gen(cfg);
    fabric::JobScheduler scheduler(2);

    LODGrid grid;
    struct GenTask {
        LODSection* section;
        int cx, cy, cz;
    };
    std::vector<GenTask> tasks;
    for (int cx = 0; cx < 4; ++cx) {
        auto* section = grid.getOrCreate(0, cx, 10, 0);
        ASSERT_NE(section, nullptr);
        tasks.push_back({section, cx, 10, 0});
    }

    scheduler.parallelFor(tasks.size(), [&tasks, &gen](size_t idx, size_t) {
        fillSectionFromWorldGen(*tasks[idx].section, gen, tasks[idx].cx, tasks[idx].cy, tasks[idx].cz);
    });

    for (int cx = 0; cx < 4; ++cx) {
        auto key = LODSectionKey::make(0, cx, 10, 0);
        auto* section = grid.get(key);
        ASSERT_NE(section, nullptr);
        bool allAir = true;
        for (int i = 0; i < LODSection::K_VOLUME; ++i) {
            if (section->materialOf(section->blockIndices[i]) != material_ids::AIR) {
                allAir = false;
                break;
            }
        }
        EXPECT_TRUE(allAir) << "Section at y=10 should be all air";
    }
}

TEST(LODGrid, Downsample_UsesAllEightChildrenAcrossParentExtent) {
    LODGrid grid;
    auto* parent = grid.getOrCreate(1, 0, 0, 0);
    ASSERT_NE(parent, nullptr);

    std::array<LODSection, 8> storage;
    std::array<LODSection*, 8> children{};
    for (int idx = 0; idx < 8; ++idx) {
        fillUniformSection(storage[static_cast<size_t>(idx)], static_cast<uint16_t>(10 + idx));
        children[static_cast<size_t>(idx)] = &storage[static_cast<size_t>(idx)];
    }

    grid.downsample(*parent, children);

    EXPECT_EQ(sectionMaterialAt(*parent, 8, 8, 8), 10);
    EXPECT_EQ(sectionMaterialAt(*parent, 24, 8, 8), 11);
    EXPECT_EQ(sectionMaterialAt(*parent, 8, 24, 8), 12);
    EXPECT_EQ(sectionMaterialAt(*parent, 24, 24, 8), 13);
    EXPECT_EQ(sectionMaterialAt(*parent, 8, 8, 24), 14);
    EXPECT_EQ(sectionMaterialAt(*parent, 24, 8, 24), 15);
    EXPECT_EQ(sectionMaterialAt(*parent, 8, 24, 24), 16);
    EXPECT_EQ(sectionMaterialAt(*parent, 24, 24, 24), 17);
}

TEST(LODGrid, GetOrCreate_UsesLevelScaledOriginAndAirDefaults) {
    LODGrid grid;
    auto* section = grid.getOrCreate(2, 3, -2, 1);
    ASSERT_NE(section, nullptr);

    constexpr int kLevel = 2;
    constexpr int kScale = 1 << kLevel;
    EXPECT_EQ(section->origin.x, 3 * LODGrid::K_SECTION_WORLD_SIZE * kScale);
    EXPECT_EQ(section->origin.y, -2 * LODGrid::K_SECTION_WORLD_SIZE * kScale);
    EXPECT_EQ(section->origin.z, 1 * LODGrid::K_SECTION_WORLD_SIZE * kScale);
    ASSERT_EQ(section->palette.size(), 1u);
    EXPECT_EQ(section->palette[0], material_ids::AIR);
    EXPECT_EQ(sectionMaterialAt(*section, 0, 0, 0), material_ids::AIR);
}

TEST(LODMeshManager, MeshSection_ScalesVerticesByLevel) {
    recurse::simulation::MaterialRegistry materials;
    LODGrid grid;
    LODMeshManager meshManager(grid, materials);

    LODSection section;
    section.level = 2;
    section.palette = {material_ids::AIR, material_ids::STONE};
    section.blockIndices.assign(LODSection::K_VOLUME, 0);
    section.set(0, 0, 0, 1);

    auto mesh = meshManager.meshSection(section);
    ASSERT_FALSE(mesh.empty());

    float maxCoord = 0.0f;
    for (const auto& vertex : mesh.vertices) {
        maxCoord = std::max({maxCoord, vertex.px, vertex.py, vertex.pz});
    }

    EXPECT_FLOAT_EQ(maxCoord, 4.0f);
}
