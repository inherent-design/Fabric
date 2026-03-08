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
  ThreadPoolExecutorTest.cc
  TimeoutLockTest.cc
  AsyncTest.cc
)
