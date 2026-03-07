# FabricShaderCompilation.cmake - Shared bgfx shader compilation function
#
# Provides fabric_compile_shader() to compile .sc shader sources to embedded
# .bin.h headers using bgfx shaderc. Called by per-domain shader modules.
#
# Requires: _SHADER_PROFILES, _SHADER_EXTS, _SHADER_PLATFORM, and
# FABRIC_BGFX_SHADER_INCLUDE set by FabricRmlUi.cmake.
include_guard()

# fabric_compile_shader(SHADER_FILE SHADER_TYPE OUT_VAR
#     SHADER_OUT_DIR <output dir>
#     VARYING_DEF <path to varying.def.sc>)
function(fabric_compile_shader SHADER_FILE SHADER_TYPE OUT_VAR)
    cmake_parse_arguments(_FSC "" "SHADER_OUT_DIR;VARYING_DEF" "" ${ARGN})

    get_filename_component(_NAME "${SHADER_FILE}" NAME_WE)
    get_filename_component(_BASENAME "${SHADER_FILE}" NAME)

    set(_OUTPUTS "")
    list(LENGTH _SHADER_PROFILES _N)
    math(EXPR _LAST "${_N} - 1")

    foreach(_IDX RANGE 0 ${_LAST})
        list(GET _SHADER_PROFILES ${_IDX} _PROFILE)
        list(GET _SHADER_EXTS ${_IDX} _EXT)

        set(_OUT_DIR "${_FSC_SHADER_OUT_DIR}/${_EXT}")
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
                --varyingdef "${_FSC_VARYING_DEF}"
                --bin2c "${_NAME}_${_EXT}"
            MAIN_DEPENDENCY "${SHADER_FILE}"
            DEPENDS "${_FSC_VARYING_DEF}"
            COMMENT "Compiling ${_BASENAME} [${_EXT}]"
        )

        list(APPEND _OUTPUTS "${_OUT_FILE}")
    endforeach()

    set(${OUT_VAR} ${_OUTPUTS} PARENT_SCOPE)
endfunction()
