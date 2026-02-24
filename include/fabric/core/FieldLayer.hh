#pragma once

#include "fabric/core/ChunkedGrid.hh"
#include "fabric/core/Spatial.hh"

namespace fabric {

template <typename T> class FieldLayer {
  public:
    T read(int x, int y, int z) const { return grid_.get(x, y, z); }
    void write(int x, int y, int z, const T& value) { grid_.set(x, y, z, value); }

    T sample(int x, int y, int z, int radius) const {
        T sum{};
        int count = 0;
        for (int dz = -radius; dz <= radius; ++dz) {
            for (int dy = -radius; dy <= radius; ++dy) {
                for (int dx = -radius; dx <= radius; ++dx) {
                    sum = sum + grid_.get(x + dx, y + dy, z + dz);
                    ++count;
                }
            }
        }
        if (count == 0)
            return T{};
        return sum * (static_cast<float>(1) / static_cast<float>(count));
    }

    void fill(int x0, int y0, int z0, int x1, int y1, int z1, const T& value) {
        for (int z = z0; z <= z1; ++z) {
            for (int y = y0; y <= y1; ++y) {
                for (int x = x0; x <= x1; ++x) {
                    grid_.set(x, y, z, value);
                }
            }
        }
    }

    ChunkedGrid<T>& grid() { return grid_; }
    const ChunkedGrid<T>& grid() const { return grid_; }

  private:
    ChunkedGrid<T> grid_;
};

using DensityField = FieldLayer<float>;
using EssenceField = FieldLayer<Vector4<float, Space::World>>;

} // namespace fabric
