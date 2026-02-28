#include "fabric/core/ParticleSystem.hh"

#include "fabric/core/Log.hh"
#include "fabric/core/Rendering.hh"
#include "fabric/utils/Profiler.hh"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <random>

// Suppress shader profiles we don't compile per-platform.
#if !defined(_WIN32)
#define BGFX_PLATFORM_SUPPORTS_DXBC 0
#endif
#define BGFX_PLATFORM_SUPPORTS_DXIL 0
#define BGFX_PLATFORM_SUPPORTS_WGSL 0
#include <bgfx/embedded_shader.h>

// Compiled shader bytecode generated at build time from .sc sources.
#include "essl/fs_particle.sc.bin.h"
#include "essl/vs_particle.sc.bin.h"
#include "glsl/fs_particle.sc.bin.h"
#include "glsl/vs_particle.sc.bin.h"
#include "spv/fs_particle.sc.bin.h"
#include "spv/vs_particle.sc.bin.h"
#if BX_PLATFORM_WINDOWS
#include "dxbc/fs_particle.sc.bin.h"
#include "dxbc/vs_particle.sc.bin.h"
#endif
#if BX_PLATFORM_OSX || BX_PLATFORM_IOS || BX_PLATFORM_VISIONOS
#include "mtl/fs_particle.sc.bin.h"
#include "mtl/vs_particle.sc.bin.h"
#endif

static const bgfx::EmbeddedShader s_particleShaders[] = {BGFX_EMBEDDED_SHADER(vs_particle),
                                                         BGFX_EMBEDDED_SHADER(fs_particle), BGFX_EMBEDDED_SHADER_END()};

// Unit quad vertices: 4 corners in [-0.5, 0.5]
static const float s_quadVertices[] = {
    -0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f, 0.5f, 0.5f, 0.0f, -0.5f, 0.5f, 0.0f,
};

static const uint16_t s_quadIndices[] = {
    0, 1, 2, 0, 2, 3,
};

namespace {

// Thread-local RNG for particle randomization
std::mt19937& rng() {
    static thread_local std::mt19937 gen{std::random_device{}()};
    return gen;
}

float randomFloat(float lo, float hi) {
    std::uniform_real_distribution<float> dist(lo, hi);
    return dist(rng());
}

} // namespace

namespace fabric {

ParticleSystem::ParticleSystem()
    : program_(BGFX_INVALID_HANDLE), vbh_(BGFX_INVALID_HANDLE), ibh_(BGFX_INVALID_HANDLE) {}

ParticleSystem::~ParticleSystem() {
    shutdown();
}

void ParticleSystem::init() {
    if (initialized_)
        return;

    bgfx::RendererType::Enum type = bgfx::getRendererType();
    program_ = bgfx::createProgram(bgfx::createEmbeddedShader(s_particleShaders, type, "vs_particle"),
                                   bgfx::createEmbeddedShader(s_particleShaders, type, "fs_particle"), true);

    bgfx::VertexLayout layout;
    layout.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float).end();

    vbh_ = bgfx::createVertexBuffer(bgfx::makeRef(s_quadVertices, sizeof(s_quadVertices)), layout);
    ibh_ = bgfx::createIndexBuffer(bgfx::makeRef(s_quadIndices, sizeof(s_quadIndices)));

    if (!bgfx::isValid(program_) || !bgfx::isValid(vbh_) || !bgfx::isValid(ibh_)) {
        FABRIC_LOG_ERROR("ParticleSystem shader/buffer init failed for renderer {}", bgfx::getRendererName(type));
        shutdown();
        return;
    }

    initialized_ = true;
    FABRIC_LOG_INFO("ParticleSystem initialized: pool={}, view={}", kMaxParticles, kViewId);
}

void ParticleSystem::shutdown() {
    if (bgfx::isValid(ibh_))
        bgfx::destroy(ibh_);
    if (bgfx::isValid(vbh_))
        bgfx::destroy(vbh_);
    if (bgfx::isValid(program_))
        bgfx::destroy(program_);

    ibh_ = BGFX_INVALID_HANDLE;
    vbh_ = BGFX_INVALID_HANDLE;
    program_ = BGFX_INVALID_HANDLE;
    initialized_ = false;
    activeCount_ = 0;
}

void ParticleSystem::initPreset(Particle& p, const Vector3<float, Space::World>& pos, float radius, ParticleType type) {
    // Base position with random offset within radius
    p.position =
        Vector3<float, Space::World>(pos.x + randomFloat(-radius, radius), pos.y + randomFloat(-radius, radius),
                                     pos.z + randomFloat(-radius, radius));
    p.age = 0.0f;
    p.type = type;

    switch (type) {
        case ParticleType::DebrisPuff:
            p.velocity = Vector3<float, Space::World>(randomFloat(-2.0f, 2.0f), randomFloat(1.0f, 4.0f),
                                                      randomFloat(-2.0f, 2.0f));
            p.colorR = randomFloat(0.5f, 0.7f);
            p.colorG = randomFloat(0.4f, 0.6f);
            p.colorB = randomFloat(0.3f, 0.5f);
            p.colorA = 1.0f;
            p.size = randomFloat(0.2f, 0.6f);
            p.rotation = randomFloat(0.0f, 6.2832f);
            p.rotationSpeed = randomFloat(-1.0f, 1.0f);
            p.lifetime = randomFloat(1.0f, 3.0f);
            p.drag = 2.0f;
            p.gravityScale = 0.3f;
            break;

        case ParticleType::AmbientDust:
            p.velocity = Vector3<float, Space::World>(randomFloat(-0.3f, 0.3f), randomFloat(-0.1f, 0.2f),
                                                      randomFloat(-0.3f, 0.3f));
            p.colorR = 0.8f;
            p.colorG = 0.8f;
            p.colorB = 0.75f;
            p.colorA = randomFloat(0.2f, 0.5f);
            p.size = randomFloat(0.05f, 0.15f);
            p.rotation = randomFloat(0.0f, 6.2832f);
            p.rotationSpeed = randomFloat(-0.3f, 0.3f);
            p.lifetime = randomFloat(3.0f, 8.0f);
            p.drag = 0.5f;
            p.gravityScale = 0.0f;
            break;

        case ParticleType::Spark:
            p.velocity = Vector3<float, Space::World>(randomFloat(-3.0f, 3.0f), randomFloat(2.0f, 6.0f),
                                                      randomFloat(-3.0f, 3.0f));
            p.colorR = 1.0f;
            p.colorG = randomFloat(0.6f, 0.9f);
            p.colorB = randomFloat(0.1f, 0.4f);
            p.colorA = 1.0f;
            p.size = randomFloat(0.03f, 0.08f);
            p.rotation = 0.0f;
            p.rotationSpeed = 0.0f;
            p.lifetime = randomFloat(0.3f, 1.0f);
            p.drag = 0.5f;
            p.gravityScale = 1.0f;
            break;
    }
}

void ParticleSystem::emit(const Vector3<float, Space::World>& pos, float radius, int count, ParticleType type) {
    for (int i = 0; i < count && activeCount_ < kMaxParticles; ++i) {
        initPreset(particles_[activeCount_], pos, radius, type);
        ++activeCount_;
    }
}

void ParticleSystem::killParticle(size_t index) {
    if (index >= activeCount_)
        return;
    --activeCount_;
    if (index < activeCount_) {
        particles_[index] = particles_[activeCount_];
    }
}

void ParticleSystem::update(float dt) {
    FABRIC_ZONE_SCOPED_N("ParticleSystem::update");

    constexpr float kGravity = 9.81f;

    size_t i = 0;
    while (i < activeCount_) {
        Particle& p = particles_[i];
        p.age += dt;

        if (p.age >= p.lifetime) {
            killParticle(i);
            continue;
        }

        // Gravity
        p.velocity.y -= kGravity * p.gravityScale * dt;

        // Drag (exponential decay)
        float dragFactor = std::max(0.0f, 1.0f - p.drag * dt);
        p.velocity.x *= dragFactor;
        p.velocity.y *= dragFactor;
        p.velocity.z *= dragFactor;

        // Integrate position
        p.position.x += p.velocity.x * dt;
        p.position.y += p.velocity.y * dt;
        p.position.z += p.velocity.z * dt;

        // Rotation
        p.rotation += p.rotationSpeed * dt;

        ++i;
    }
}

void ParticleSystem::render(const float* viewMtx, const float* projMtx, uint16_t width, uint16_t height) {
    FABRIC_ZONE_SCOPED_N("ParticleSystem::render");

    if (!isValid() || activeCount_ == 0)
        return;

    // Check instancing support (need at least 3 vec4s = 48 bytes)
    const uint16_t instanceStride = static_cast<uint16_t>(kInstanceStride);
    if (bgfx::getAvailInstanceDataBuffer(static_cast<uint32_t>(activeCount_), instanceStride) == 0)
        return;

    // Configure particle view
    bgfx::setViewRect(kViewId, 0, 0, width, height);
    bgfx::setViewTransform(kViewId, viewMtx, projMtx);

    // Allocate transient instance data buffer
    bgfx::InstanceDataBuffer idb;
    bgfx::allocInstanceDataBuffer(&idb, static_cast<uint32_t>(activeCount_), instanceStride);

    uint8_t* data = idb.data;
    for (size_t i = 0; i < activeCount_; ++i) {
        const Particle& p = particles_[i];
        float ageRatio = p.age / p.lifetime;

        // Fade out alpha over lifetime
        float alpha = p.colorA * (1.0f - ageRatio * ageRatio);

        float* inst = reinterpret_cast<float*>(data);
        // i_data0: position.xyz + size
        inst[0] = p.position.x;
        inst[1] = p.position.y;
        inst[2] = p.position.z;
        inst[3] = p.size * (1.0f + ageRatio * 0.5f); // grow slightly over time

        // i_data1: color rgba
        inst[4] = p.colorR;
        inst[5] = p.colorG;
        inst[6] = p.colorB;
        inst[7] = alpha;

        // i_data2: rotation, ageRatio, pad, pad
        inst[8] = p.rotation;
        inst[9] = ageRatio;
        inst[10] = 0.0f;
        inst[11] = 0.0f;

        data += instanceStride;
    }

    bgfx::setVertexBuffer(0, vbh_);
    bgfx::setIndexBuffer(ibh_);
    bgfx::setInstanceDataBuffer(&idb);

    // Alpha blend, depth test but no depth write (particles render behind opaque)
    uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_LESS |
                     BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA);

    bgfx::setState(state);
    bgfx::submit(kViewId, program_);
}

size_t ParticleSystem::activeCount() const {
    return activeCount_;
}

bool ParticleSystem::isValid() const {
    return bgfx::isValid(program_);
}

} // namespace fabric
