#include "fabric/render/DrawCall.hh"
#include "fabric/utils/Profiler.hh"
#include <algorithm>

namespace fabric {

// RenderList

void RenderList::addDrawCall(const DrawCall& call) {
    drawCalls_.push_back(call);
}

void RenderList::sortByKey() {
    FABRIC_ZONE_SCOPED_N("RenderList::sortByKey");
    std::sort(drawCalls_.begin(), drawCalls_.end(),
              [](const DrawCall& a, const DrawCall& b) { return a.sortKey < b.sortKey; });
}

void RenderList::clear() {
    drawCalls_.clear();
}

const std::vector<DrawCall>& RenderList::drawCalls() const {
    return drawCalls_;
}

size_t RenderList::size() const {
    return drawCalls_.size();
}

bool RenderList::empty() const {
    return drawCalls_.empty();
}

// TransformInterpolator

Transform<float> TransformInterpolator::interpolate(const Transform<float>& prev, const Transform<float>& current,
                                                    float alpha) {
    Transform<float> result;

    result.setPosition(Vector3<float, Space::World>::lerp(prev.getPosition(), current.getPosition(), alpha));

    result.setRotation(Quaternion<float>::slerp(prev.getRotation(), current.getRotation(), alpha));

    result.setScale(Vector3<float, Space::World>::lerp(prev.getScale(), current.getScale(), alpha));

    return result;
}

} // namespace fabric
