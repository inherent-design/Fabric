# FabricSmoothShaders.cmake - Compile voxel-lighting shaders (Journey style)
# Unified from former smooth + LOD shaders (identical pipelines).
include_guard()
include(FabricShaderCompilation)

set(FABRIC_SMOOTH_SHADER_DIR "${CMAKE_SOURCE_DIR}/shaders/voxel-lighting")
set(FABRIC_SMOOTH_SHADER_OUT "${CMAKE_BINARY_DIR}/generated/shaders/smooth")
set(_SMOOTH_VARYING "${CMAKE_SOURCE_DIR}/shaders/shared/varying_voxel_lighting.def.sc")

fabric_compile_shader("${FABRIC_SMOOTH_SHADER_DIR}/vs_smooth.sc" vertex SMOOTH_VS_OUTPUTS
    SHADER_OUT_DIR "${FABRIC_SMOOTH_SHADER_OUT}" VARYING_DEF "${_SMOOTH_VARYING}")
fabric_compile_shader("${FABRIC_SMOOTH_SHADER_DIR}/vs_voxel.sc" vertex VOXEL_VS_OUTPUTS
    SHADER_OUT_DIR "${FABRIC_SMOOTH_SHADER_OUT}" VARYING_DEF "${_SMOOTH_VARYING}")
fabric_compile_shader("${FABRIC_SMOOTH_SHADER_DIR}/fs_smooth.sc" fragment SMOOTH_FS_OUTPUTS
    SHADER_OUT_DIR "${FABRIC_SMOOTH_SHADER_OUT}" VARYING_DEF "${_SMOOTH_VARYING}")

add_custom_target(FabricSmoothShaders DEPENDS ${SMOOTH_VS_OUTPUTS} ${VOXEL_VS_OUTPUTS} ${SMOOTH_FS_OUTPUTS})
