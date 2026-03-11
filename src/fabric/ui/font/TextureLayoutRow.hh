#pragma once

#include "TextureLayoutRectangle.hh"
#include <RmlUi/Core.h>

namespace fabric::ui::font {

class TextureLayout;

class TextureLayoutRow {
  public:
    TextureLayoutRow();
    ~TextureLayoutRow();

    int Generate(TextureLayout& layout, int width, int y);
    void Allocate(Rml::byte* texture_data, int stride);
    int GetHeight() const;
    void Unplace();

  private:
    using RectangleList = Rml::Vector<TextureLayoutRectangle*>;

    int height;
    RectangleList rectangles;
};

} // namespace fabric::ui::font
