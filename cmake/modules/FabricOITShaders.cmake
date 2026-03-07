# FabricOITShaders.cmake - Compile OIT (order-independent transparency) shaders
include_guard()
include(FabricShaderCompilation)

set(FABRIC_OIT_SHADER_DIR "${CMAKE_SOURCE_DIR}/shaders/oit")
set(FABRIC_OIT_SHADER_OUT "${CMAKE_BINARY_DIR}/generated/shaders/oit")
set(_OIT_VARYING "${CMAKE_SOURCE_DIR}/shaders/shared/varying_fullscreen.def.sc")

fabric_compile_shader("${FABRIC_OIT_SHADER_DIR}/vs_oit_accum.sc" vertex OIT_VS_ACCUM_OUTPUTS
    SHADER_OUT_DIR "${FABRIC_OIT_SHADER_OUT}" VARYING_DEF "${_OIT_VARYING}")
fabric_compile_shader("${FABRIC_OIT_SHADER_DIR}/fs_oit_accum.sc" fragment OIT_FS_ACCUM_OUTPUTS
    SHADER_OUT_DIR "${FABRIC_OIT_SHADER_OUT}" VARYING_DEF "${_OIT_VARYING}")
fabric_compile_shader("${CMAKE_SOURCE_DIR}/shaders/shared/vs_fullscreen.sc" vertex OIT_VS_COMPOSITE_OUTPUTS
    SHADER_OUT_DIR "${FABRIC_OIT_SHADER_OUT}" VARYING_DEF "${_OIT_VARYING}")
fabric_compile_shader("${FABRIC_OIT_SHADER_DIR}/fs_oit_composite.sc" fragment OIT_FS_COMPOSITE_OUTPUTS
    SHADER_OUT_DIR "${FABRIC_OIT_SHADER_OUT}" VARYING_DEF "${_OIT_VARYING}")

add_custom_target(FabricOITShaders DEPENDS
    ${OIT_VS_ACCUM_OUTPUTS}
    ${OIT_FS_ACCUM_OUTPUTS}
    ${OIT_VS_COMPOSITE_OUTPUTS}
    ${OIT_FS_COMPOSITE_OUTPUTS}
)
