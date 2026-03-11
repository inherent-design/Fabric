#pragma once

#include <RmlUi/Core.h>

namespace fabric::ui::font {

class TextureLayoutRectangle {
  public:
    TextureLayoutRectangle(uint64_t id, Rml::Vector2i dimensions);
    ~TextureLayoutRectangle();

    uint64_t GetId() const;
    Rml::Vector2i GetPosition() const;
    Rml::Vector2i GetDimensions() const;

    void Place(int texture_index, Rml::Vector2i position);
    void Unplace();
    bool IsPlaced() const;

    void Allocate(Rml::byte* texture_data, int texture_stride);

    int GetTextureIndex();
    Rml::byte* GetTextureData();
    int GetTextureStride() const;

  private:
    uint64_t id;
    Rml::Vector2i dimensions;

    int texture_index;
    Rml::Vector2i texture_position;

    Rml::byte* texture_data;
    int texture_stride;
};

} // namespace fabric::ui::font
