#pragma once

#include "TextureLayoutRow.hh"
#include <RmlUi/Core.h>

namespace fabric::ui::font {

class TextureLayout;

class TextureLayoutTexture {
  public:
    TextureLayoutTexture();
    ~TextureLayoutTexture();

    Rml::Vector2i GetDimensions() const;
    int Generate(TextureLayout& layout, int maximum_dimensions);
    Rml::Vector<Rml::byte> AllocateTexture();

  private:
    using RowList = Rml::Vector<TextureLayoutRow>;

    Rml::Vector2i dimensions;
    RowList rows;
};

} // namespace fabric::ui::font
