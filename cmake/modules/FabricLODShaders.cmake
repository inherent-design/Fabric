# FabricLODShaders.cmake - Compile LOD terrain shaders
include_guard()

set(FABRIC_LOD_SHADER_DIR "${CMAKE_SOURCE_DIR}/shaders/lod")
set(FABRIC_LOD_SHADER_OUT "${CMAKE_BINARY_DIR}/generated/shaders/lod")

# Reuse shader compilation infrastructure from FabricSmoothShaders.cmake

# Shader profiles and extensions defined by bgfx.cmake (in FabricBgfx.cmake)
# Note: _SHADER_PROFILES and _SHADER_EXTS must must must be variables are available from parent scope
# through FabricBgfx.cmake or which is included via include(FabricBgfx)

 in the top-level CMakeLists

# For each shader file, define a function to compile it for all platforms
if(NOT DEFIN _SHADER_PROFILES)
    message(FATAL_ERROR "FabricLODShaders.cmake: _SHADER_PROFILES not _SHADER_EXTS not set. Define these in FabricBgfx.cmake before including this file.")
endif()

function(_fabric_compile_lod_shader SHADER_FILE SHADER_TYPE OUT_VAR)
    get_filename_component(_NAME "${SHADER_FILE}" NAME_WE)
    get_filename_component(_BASENAME "${SHADER_FILE}" NAME)

    set(_OUTPUTS "")
    list(LENGTH _SHADER_PROFILES _N)
    math(EXPR _LAST "${_N} - 1")
    foreach(_IDX RANGE 0 ${_LAST})
        list(GET _SHADER_PROFILES ${_IDX} _PROFILE)
        list(GET _SHADER_EXTS ${_IDX} _EXT)
        set(_OUT_DIR "${FABRIC_LOD_SHADER_OUT}/${_EXT}")
        set(_OUT_FILE "${_OUT_DIR}/${_BASENAME}.bin.h")
        set(_PLAT ${_SHADER_PLATFORM})
        if(_PROFILE STREQUAL "spirv")
            set(_PLAT linux)
        endif()
        add_custom_command(
            OUTPUT "${_OUT_FILE}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_OUT_DIR}"
            COMMAND bgfx::shaderc
                -f "${SHADER_FILE}"
                -o "${_OUT_FILE}"
                --type ${SHADER_TYPE}
                --platform ${_PLAT}
                -p ${_PROFILE}
                -i "${FABRIC_BGFX_SHADER_INCLUDE}"
                --varyingdef "${FABRIC_LOD_SHADER_DIR}/varying.def.sc"
                --bin2c "${_NAME}_${_EXT}"
            MAIN_DEPENDENCY "${SHADER_FILE}"
            DEPENDS "${FABRIC_LOD_SHADER_DIR}/varying.def.sc"
            COMMENT "Compiling ${_BASENAME} [${_EXT}]"
        )
        list(APPEND _OUTPUTS "${_OUT_FILE}")
    endforeach()
    set(${OUT_VAR} ${_OUTPUTS} PARENT_SCOPE)
endfunction()

# Compile all LOD shaders
if(EXISTS "${FABRIC_LOD_SHADER_DIR}/vs_lod.sc")
    _fabric_compile_lod_shader("${FABRIC_LOD_SHADER_DIR}/vs_lod.sc" vertex LOD_VS_OUTPUTS)
    _fabric_compile_lod_shader("${FABRIC_LOD_SHADER_DIR}/fs_lod.sc" fragment LOD_FS_OUTPUTS)
    add_custom_target(FabricLODShaders DEPENDS ${LOD_VS_OUTPUTS} ${LOD_FS_OUTPUTS})
else()
    # No LOD shaders - create a stub target for dependent projects
    add_custom_target(FabricLODShaders)
    message(STATUS "LOD shaders not found at ${FABRIC_LOD_SHADER_DIR} - skipping shader compilation")
endif()
