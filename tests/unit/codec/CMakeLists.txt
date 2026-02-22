# Codec unit tests
target_sources(UnitTests PRIVATE CodecTest.cc)
set_source_files_properties(CodecTest.cc PROPERTIES COMPILE_DEFINITIONS "FABRIC_TEST")
