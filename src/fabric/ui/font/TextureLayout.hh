#pragma once

#include "TextureLayoutRectangle.hh"
#include "TextureLayoutTexture.hh"
#include <RmlUi/Core.h>

namespace fabric::ui::font {

class TextureLayout {
  public:
    TextureLayout();
    ~TextureLayout();

    void AddRectangle(uint64_t id, Rml::Vector2i dimensions);

    TextureLayoutRectangle& GetRectangle(int index);
    int GetNumRectangles() const;

    TextureLayoutTexture& GetTexture(int index);
    int GetNumTextures() const;

    bool GenerateLayout(int max_texture_dimensions);

  private:
    using RectangleList = Rml::Vector<TextureLayoutRectangle>;
    using TextureList = Rml::Vector<TextureLayoutTexture>;

    TextureList textures;
    RectangleList rectangles;
};

} // namespace fabric::ui::font
