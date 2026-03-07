# FabricPostShaders.cmake - Compile post-process shaders
include_guard()
include(FabricShaderCompilation)

set(FABRIC_POST_SHADER_DIR "${CMAKE_SOURCE_DIR}/shaders/post")
set(FABRIC_POST_SHADER_OUT "${CMAKE_BINARY_DIR}/generated/shaders/post")
set(_POST_VARYING "${CMAKE_SOURCE_DIR}/shaders/shared/varying_fullscreen.def.sc")

fabric_compile_shader("${CMAKE_SOURCE_DIR}/shaders/shared/vs_fullscreen.sc" vertex POST_VS_OUTPUTS
    SHADER_OUT_DIR "${FABRIC_POST_SHADER_OUT}" VARYING_DEF "${_POST_VARYING}")
fabric_compile_shader("${FABRIC_POST_SHADER_DIR}/fs_bright.sc" fragment POST_FS_BRIGHT_OUTPUTS
    SHADER_OUT_DIR "${FABRIC_POST_SHADER_OUT}" VARYING_DEF "${_POST_VARYING}")
fabric_compile_shader("${FABRIC_POST_SHADER_DIR}/fs_blur.sc" fragment POST_FS_BLUR_OUTPUTS
    SHADER_OUT_DIR "${FABRIC_POST_SHADER_OUT}" VARYING_DEF "${_POST_VARYING}")
fabric_compile_shader("${FABRIC_POST_SHADER_DIR}/fs_tonemap.sc" fragment POST_FS_TONEMAP_OUTPUTS
    SHADER_OUT_DIR "${FABRIC_POST_SHADER_OUT}" VARYING_DEF "${_POST_VARYING}")

add_custom_target(FabricPostShaders DEPENDS
    ${POST_VS_OUTPUTS}
    ${POST_FS_BRIGHT_OUTPUTS}
    ${POST_FS_BLUR_OUTPUTS}
    ${POST_FS_TONEMAP_OUTPUTS}
)
