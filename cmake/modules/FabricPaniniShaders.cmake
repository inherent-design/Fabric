# FabricPaniniShaders.cmake - Compile Panini projection post-process shaders
include_guard()
include(FabricShaderCompilation)

set(FABRIC_PANINI_SHADER_DIR "${CMAKE_SOURCE_DIR}/shaders/panini")
set(FABRIC_PANINI_SHADER_OUT "${CMAKE_BINARY_DIR}/generated/shaders/panini")
set(_PANINI_VARYING "${CMAKE_SOURCE_DIR}/shaders/shared/varying_fullscreen.def.sc")

fabric_compile_shader("${CMAKE_SOURCE_DIR}/shaders/shared/vs_fullscreen.sc" vertex PANINI_VS_OUTPUTS
    SHADER_OUT_DIR "${FABRIC_PANINI_SHADER_OUT}" VARYING_DEF "${_PANINI_VARYING}")
fabric_compile_shader("${FABRIC_PANINI_SHADER_DIR}/fs_panini.sc" fragment PANINI_FS_OUTPUTS
    SHADER_OUT_DIR "${FABRIC_PANINI_SHADER_OUT}" VARYING_DEF "${_PANINI_VARYING}")

add_custom_target(FabricPaniniShaders DEPENDS
    ${PANINI_VS_OUTPUTS}
    ${PANINI_FS_OUTPUTS}
)
