# FabricSkinnedShaders.cmake - Compile skinned mesh shaders
include_guard()
include(FabricShaderCompilation)

set(FABRIC_SKINNED_SHADER_DIR "${CMAKE_SOURCE_DIR}/shaders/skinned")
set(FABRIC_SKINNED_SHADER_OUT "${CMAKE_BINARY_DIR}/generated/shaders/skinned")

fabric_compile_shader("${FABRIC_SKINNED_SHADER_DIR}/vs_skinned.sc" vertex SKINNED_VS_OUTPUTS
    SHADER_OUT_DIR "${FABRIC_SKINNED_SHADER_OUT}" VARYING_DEF "${FABRIC_SKINNED_SHADER_DIR}/varying.def.sc")
fabric_compile_shader("${FABRIC_SKINNED_SHADER_DIR}/fs_skinned.sc" fragment SKINNED_FS_OUTPUTS
    SHADER_OUT_DIR "${FABRIC_SKINNED_SHADER_OUT}" VARYING_DEF "${FABRIC_SKINNED_SHADER_DIR}/varying.def.sc")

add_custom_target(FabricSkinnedShaders DEPENDS ${SKINNED_VS_OUTPUTS} ${SKINNED_FS_OUTPUTS})
