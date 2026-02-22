# Core unit tests
target_sources(UnitTests
  PRIVATE
  ComponentTest.cc
  CoreApiTest.cc
  EventTest.cc
  ResourceHubTest.cc
  LifecycleTest.cc
  PluginTest.cc
  CommandTest.cc
  ResourceTest.cc
  SpatialTest.cc
  StateMachineTest.cc
  TemporalTest.cc
  JsonTypesTest.cc
  PipelineTest.cc
  RenderingTest.cc
)

set_source_files_properties(
  CommandTest.cc
  LifecycleTest.cc
  PluginTest.cc
  ResourceHubTest.cc
  ResourceTest.cc
  SpatialTest.cc
  TemporalTest.cc
  PipelineTest.cc
  PROPERTIES
  COMPILE_DEFINITIONS "FABRIC_TEST"
)
