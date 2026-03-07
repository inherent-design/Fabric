# FabricParticleShaders.cmake - Compile particle shaders
include_guard()
include(FabricShaderCompilation)

set(FABRIC_PARTICLE_SHADER_DIR "${CMAKE_SOURCE_DIR}/shaders/particle")
set(FABRIC_PARTICLE_SHADER_OUT "${CMAKE_BINARY_DIR}/generated/shaders/particle")

fabric_compile_shader("${FABRIC_PARTICLE_SHADER_DIR}/vs_particle.sc" vertex PARTICLE_VS_OUTPUTS
    SHADER_OUT_DIR "${FABRIC_PARTICLE_SHADER_OUT}" VARYING_DEF "${FABRIC_PARTICLE_SHADER_DIR}/varying.def.sc")
fabric_compile_shader("${FABRIC_PARTICLE_SHADER_DIR}/fs_particle.sc" fragment PARTICLE_FS_OUTPUTS
    SHADER_OUT_DIR "${FABRIC_PARTICLE_SHADER_OUT}" VARYING_DEF "${FABRIC_PARTICLE_SHADER_DIR}/varying.def.sc")

add_custom_target(FabricParticleShaders DEPENDS ${PARTICLE_VS_OUTPUTS} ${PARTICLE_FS_OUTPUTS})
