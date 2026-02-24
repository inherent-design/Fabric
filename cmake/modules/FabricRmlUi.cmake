# RmlUi - HTML/CSS UI library with vertex-level rendering control
# Includes CPM fetch for RmlUi 6.2 and build-time shader compilation
# for the bgfx RenderInterface.
include_guard()

#------------------------------------------------------------------------------
# FreeType (required by RmlUi font engine)
# Use system package on Linux/macOS; build from source on Windows.
#------------------------------------------------------------------------------
find_package(Freetype QUIET)
if(NOT FREETYPE_FOUND)
    message(STATUS "System Freetype not found - building from source via CPM")
    CPMAddPackage(
        NAME freetype
        GITHUB_REPOSITORY freetype/freetype
        GIT_TAG VER-2-14-1
        OPTIONS
            "FT_DISABLE_HARFBUZZ ON"
            "FT_DISABLE_BZIP2 ON"
            "FT_DISABLE_BROTLI ON"
            "FT_DISABLE_PNG ON"
        SYSTEM
        EXCLUDE_FROM_ALL
    )

    # Populate find_package(Freetype) variables so RmlUi's internal
    # find_package(Freetype) succeeds without re-searching.
    set(FREETYPE_FOUND TRUE CACHE BOOL "" FORCE)
    set(FREETYPE_INCLUDE_DIRS "${freetype_SOURCE_DIR}/include" CACHE PATH "" FORCE)
    set(FREETYPE_LIBRARIES freetype CACHE STRING "" FORCE)
    if(NOT TARGET Freetype::Freetype)
        add_library(Freetype::Freetype ALIAS freetype)
    endif()
endif()

#------------------------------------------------------------------------------
# RmlUi
#------------------------------------------------------------------------------

# Preserve BUILD_SHARED_LIBS state: RmlUi may flip it to ON
set(_FABRIC_SAVED_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF)

CPMAddPackage(
    NAME RmlUi
    GITHUB_REPOSITORY mikke89/RmlUi
    GIT_TAG 6.2
    OPTIONS
        "RMLUI_SAMPLES OFF"
        "RMLUI_TESTS OFF"
        "RMLUI_THIRDPARTY_CONTAINERS ON"
        "RMLUI_FONT_ENGINE freetype"
    SYSTEM
    EXCLUDE_FROM_ALL
)

# Restore BUILD_SHARED_LIBS
set(BUILD_SHARED_LIBS ${_FABRIC_SAVED_BUILD_SHARED_LIBS})

#------------------------------------------------------------------------------
# RmlUi bgfx shader compilation
#------------------------------------------------------------------------------
# Compile .sc shader sources to embedded .bin.h headers using bgfx shaderc.
# Produces per-profile arrays compatible with BGFX_EMBEDDED_SHADER macro.

set(FABRIC_RMLUI_SHADER_DIR "${CMAKE_SOURCE_DIR}/shaders/rmlui")
set(FABRIC_RMLUI_SHADER_OUT "${CMAKE_BINARY_DIR}/generated/shaders/rmlui")
set(FABRIC_BGFX_SHADER_INCLUDE "${bgfx_SOURCE_DIR}/bgfx/src")

# Platform profiles (must match embedded_shader.h expectations)
if(APPLE AND NOT IOS)
    set(_SHADER_PLATFORM osx)
    set(_SHADER_PROFILES "metal;120;300_es;spirv")
    set(_SHADER_EXTS     "mtl;glsl;essl;spv")
elseif(IOS)
    set(_SHADER_PLATFORM ios)
    set(_SHADER_PROFILES "metal;300_es;spirv")
    set(_SHADER_EXTS     "mtl;essl;spv")
elseif(WIN32)
    set(_SHADER_PLATFORM windows)
    set(_SHADER_PROFILES "s_5_0;120;300_es;spirv")
    set(_SHADER_EXTS     "dxbc;glsl;essl;spv")
elseif(UNIX)
    set(_SHADER_PLATFORM linux)
    set(_SHADER_PROFILES "120;300_es;spirv")
    set(_SHADER_EXTS     "glsl;essl;spv")
endif()

function(_fabric_compile_shader SHADER_FILE SHADER_TYPE OUT_VAR)
    get_filename_component(_NAME "${SHADER_FILE}" NAME_WE)
    get_filename_component(_BASENAME "${SHADER_FILE}" NAME)

    set(_OUTPUTS "")
    list(LENGTH _SHADER_PROFILES _N)
    math(EXPR _LAST "${_N} - 1")

    foreach(_IDX RANGE 0 ${_LAST})
        list(GET _SHADER_PROFILES ${_IDX} _PROFILE)
        list(GET _SHADER_EXTS ${_IDX} _EXT)

        set(_OUT_DIR "${FABRIC_RMLUI_SHADER_OUT}/${_EXT}")
        set(_OUT_FILE "${_OUT_DIR}/${_BASENAME}.bin.h")

        # SPIR-V uses linux platform for shaderc
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
                --varyingdef "${FABRIC_RMLUI_SHADER_DIR}/varying.def.sc"
                --bin2c "${_NAME}_${_EXT}"
            MAIN_DEPENDENCY "${SHADER_FILE}"
            DEPENDS "${FABRIC_RMLUI_SHADER_DIR}/varying.def.sc"
            COMMENT "Compiling ${_BASENAME} [${_EXT}]"
        )

        list(APPEND _OUTPUTS "${_OUT_FILE}")
    endforeach()

    set(${OUT_VAR} ${_OUTPUTS} PARENT_SCOPE)
endfunction()

_fabric_compile_shader("${FABRIC_RMLUI_SHADER_DIR}/vs_rmlui.sc" vertex VS_OUTPUTS)
_fabric_compile_shader("${FABRIC_RMLUI_SHADER_DIR}/fs_rmlui.sc" fragment FS_OUTPUTS)

add_custom_target(FabricRmlUiShaders DEPENDS ${VS_OUTPUTS} ${FS_OUTPUTS})
