#pragma once

#include "fabric/core/Spatial.hh"
#include <array>
#include <cstdint>
#include <vector>

namespace fabric {

// Draw call for bgfx submission
struct DrawCall {
    uint64_t sortKey = 0;
    std::array<float, 16> transform = {};
    // bgfx handles stored as uint16_t to avoid bgfx header dependency
    uint16_t program = 0;
    uint16_t vertexBuffer = 0;
    uint16_t indexBuffer = 0;
    uint32_t indexCount = 0;
    uint32_t indexOffset = 0;
    uint8_t viewId = 0;
};

// Sorted collection of draw calls per view
class RenderList {
  public:
    void addDrawCall(const DrawCall& call);
    void sortByKey();
    void clear();

    const std::vector<DrawCall>& drawCalls() const;
    size_t size() const;
    bool empty() const;

  private:
    std::vector<DrawCall> drawCalls_;
};

// Transform interpolation using slerp (rotation) + lerp (position, scale)
struct TransformInterpolator {
    static Transform<float> interpolate(const Transform<float>& prev, const Transform<float>& current, float alpha);
};

} // namespace fabric
