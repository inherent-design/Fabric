#include "TextureLayoutRow.hh"
#include "TextureLayout.hh"

namespace fabric::ui::font {

TextureLayoutRow::TextureLayoutRow() {
    height = 0;
}

TextureLayoutRow::~TextureLayoutRow() {}

int TextureLayoutRow::Generate(TextureLayout& layout, int max_width, int y) {
    int width = 1;
    int first_unplaced_index = 0;
    int placed_rectangles = 0;

    while (width < max_width) {
        int index;
        for (index = first_unplaced_index; index < layout.GetNumRectangles(); ++index) {
            TextureLayoutRectangle& rectangle = layout.GetRectangle(index);
            if (!rectangle.IsPlaced()) {
                if (width + rectangle.GetDimensions().x + 1 <= max_width)
                    break;
            }
        }

        if (index == layout.GetNumRectangles())
            return placed_rectangles;

        TextureLayoutRectangle& rectangle = layout.GetRectangle(index);

        height = Rml::Math::Max(height, rectangle.GetDimensions().y);

        rectangles.push_back(&rectangle);
        rectangle.Place(layout.GetNumTextures(), Rml::Vector2i(width, y));
        ++placed_rectangles;

        if (rectangle.GetDimensions().x > 0)
            width += rectangle.GetDimensions().x + 1;

        first_unplaced_index = index + 1;
    }

    return placed_rectangles;
}

void TextureLayoutRow::Allocate(Rml::byte* texture_data, int stride) {
    for (size_t i = 0; i < rectangles.size(); ++i)
        rectangles[i]->Allocate(texture_data, stride);
}

int TextureLayoutRow::GetHeight() const {
    return height;
}

void TextureLayoutRow::Unplace() {
    for (size_t i = 0; i < rectangles.size(); ++i)
        rectangles[i]->Unplace();
}

} // namespace fabric::ui::font
