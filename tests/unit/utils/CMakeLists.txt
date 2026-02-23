# Utils unit tests
target_sources(UnitTests
  PRIVATE
  UtilsTest.cc
  ErrorHandlingTest.cc
  LoggingTest.cc
  CoordinatedGraphTest.cc
  BufferPoolTest.cc
  ImmutableDAGTest.cc
  BVHTest.cc
)

set_source_files_properties(
  UtilsTest.cc
  ErrorHandlingTest.cc
  LoggingTest.cc
  CoordinatedGraphTest.cc
  BufferPoolTest.cc
  ImmutableDAGTest.cc
  BVHTest.cc
  PROPERTIES
  COMPILE_DEFINITIONS "FABRIC_TEST"
)
