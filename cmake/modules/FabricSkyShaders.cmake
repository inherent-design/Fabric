# FabricSkyShaders.cmake - Compile sky shaders
include_guard()
include(FabricShaderCompilation)

set(FABRIC_SKY_SHADER_DIR "${CMAKE_SOURCE_DIR}/shaders/sky")
set(FABRIC_SKY_SHADER_OUT "${CMAKE_BINARY_DIR}/generated/shaders/sky")

fabric_compile_shader("${FABRIC_SKY_SHADER_DIR}/vs_sky.sc" vertex SKY_VS_OUTPUTS
    SHADER_OUT_DIR "${FABRIC_SKY_SHADER_OUT}" VARYING_DEF "${FABRIC_SKY_SHADER_DIR}/varying.def.sc")
fabric_compile_shader("${FABRIC_SKY_SHADER_DIR}/fs_sky.sc" fragment SKY_FS_OUTPUTS
    SHADER_OUT_DIR "${FABRIC_SKY_SHADER_OUT}" VARYING_DEF "${FABRIC_SKY_SHADER_DIR}/varying.def.sc")

add_custom_target(FabricSkyShaders DEPENDS ${SKY_VS_OUTPUTS} ${SKY_FS_OUTPUTS})
