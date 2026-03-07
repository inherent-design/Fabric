target_sources(UnitTests
  PRIVATE
  MaterialRegistryTest.cc
  SimulationGridTest.cc
  ChunkActivityTrackerTest.cc
  GhostCellsTest.cc
  FallingSandGravityTest.cc
  FallingSandLiquidTest.cc
  VoxelSimulationSystemTest.cc
  ParallelSimulationTest.cc
)

set_source_files_properties(
  MaterialRegistryTest.cc
  SimulationGridTest.cc
  ChunkActivityTrackerTest.cc
  GhostCellsTest.cc
  FallingSandGravityTest.cc
  FallingSandLiquidTest.cc
  VoxelSimulationSystemTest.cc
  ParallelSimulationTest.cc
  PROPERTIES
  COMPILE_DEFINITIONS "FABRIC_TEST"
)
