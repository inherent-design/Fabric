target_sources(UnitTests
  PRIVATE
  GeneratorTest.cc
  MinecraftNoiseGeneratorTest.cc
)

set_source_files_properties(
  GeneratorTest.cc
  MinecraftNoiseGeneratorTest.cc
  PROPERTIES
  COMPILE_DEFINITIONS "FABRIC_TEST"
)
