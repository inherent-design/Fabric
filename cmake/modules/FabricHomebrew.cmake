# FabricHomebrew.cmake - Shared Homebrew LLVM rpath fix
#
# Homebrew LLVM ships a newer libc++ than the macOS system; targets need an
# rpath to the Homebrew lib/c++ directory so symbols like __hash_memory resolve.
include_guard()

function(fabric_fix_homebrew_llvm_rpath TARGET)
    if(APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND NOT CMAKE_CXX_COMPILER MATCHES "/usr/bin")
        get_filename_component(_compiler_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
        get_filename_component(_llvm_root "${_compiler_dir}" DIRECTORY)
        set(_libcxx_path "${_llvm_root}/lib/c++")
        if(EXISTS "${_libcxx_path}/libc++.dylib")
            target_link_options(${TARGET} PUBLIC "-L${_libcxx_path}" "-Wl,-rpath,${_libcxx_path}")
        endif()
    endif()
endfunction()
