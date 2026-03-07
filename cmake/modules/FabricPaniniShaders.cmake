# FabricPaniniShaders.cmake - Compile Panini projection post-process shaders
include_guard()

set(FABRIC_PANINI_SHADER_DIR "${CMAKE_SOURCE_DIR}/shaders/panini")
set(FABRIC_PANINI_SHADER_OUT "${CMAKE_BINARY_DIR}/generated/shaders/panini")

# Reuse shader compilation infrastructure from FabricRmlUi.cmake
# (platform profiles, bgfx include path, shaderc target are set there)

function(_fabric_compile_panini_shader SHADER_FILE SHADER_TYPE OUT_VAR)
    get_filename_component(_NAME "${SHADER_FILE}" NAME_WE)
    get_filename_component(_BASENAME "${SHADER_FILE}" NAME)

    set(_OUTPUTS "")
    list(LENGTH _SHADER_PROFILES _N)
    math(EXPR _LAST "${_N} - 1")

    foreach(_IDX RANGE 0 ${_LAST})
        list(GET _SHADER_PROFILES ${_IDX} _PROFILE)
        list(GET _SHADER_EXTS ${_IDX} _EXT)

        set(_OUT_DIR "${FABRIC_PANINI_SHADER_OUT}/${_EXT}")
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
                --varyingdef "${FABRIC_PANINI_SHADER_DIR}/varying.def.sc"
                --bin2c "${_NAME}_${_EXT}"
            MAIN_DEPENDENCY "${SHADER_FILE}"
            DEPENDS "${FABRIC_PANINI_SHADER_DIR}/varying.def.sc"
            COMMENT "Compiling ${_BASENAME} [${_EXT}]"
        )

        list(APPEND _OUTPUTS "${_OUT_FILE}")
    endforeach()

    set(${OUT_VAR} ${_OUTPUTS} PARENT_SCOPE)
endfunction()

_fabric_compile_panini_shader("${FABRIC_PANINI_SHADER_DIR}/vs_panini.sc" vertex PANINI_VS_OUTPUTS)
_fabric_compile_panini_shader("${FABRIC_PANINI_SHADER_DIR}/fs_panini.sc" fragment PANINI_FS_OUTPUTS)

add_custom_target(FabricPaniniShaders DEPENDS
    ${PANINI_VS_OUTPUTS}
    ${PANINI_FS_OUTPUTS}
)
