#include "TextureLayoutTexture.hh"
#include "TextureLayout.hh"

namespace fabric::ui::font {

TextureLayoutTexture::TextureLayoutTexture() : dimensions(0, 0) {}

TextureLayoutTexture::~TextureLayoutTexture() {}

Rml::Vector2i TextureLayoutTexture::GetDimensions() const {
    return dimensions;
}

int TextureLayoutTexture::Generate(TextureLayout& layout, int maximum_dimensions) {
    int square_pixels = 0;
    int unplaced_rectangles = 0;
    for (int i = 0; i < layout.GetNumRectangles(); ++i) {
        const TextureLayoutRectangle& rectangle = layout.GetRectangle(i);

        if (!rectangle.IsPlaced()) {
            int x = rectangle.GetDimensions().x + 1;
            int y = rectangle.GetDimensions().y + 1;

            square_pixels += x * y;
            ++unplaced_rectangles;
        }
    }

    int texture_width = int(Rml::Math::SquareRoot((float)square_pixels));

    dimensions.y = Rml::Math::ToPowerOfTwo(texture_width);
    dimensions.x = dimensions.y >> 1;

    dimensions.x = Rml::Math::Min(dimensions.x, maximum_dimensions);
    dimensions.y = Rml::Math::Min(dimensions.y, maximum_dimensions);

    int num_placed_rectangles = 0;
    for (;;) {
        bool success = true;
        int height = 1;

        while (num_placed_rectangles != unplaced_rectangles) {
            TextureLayoutRow row;
            int row_size = row.Generate(layout, dimensions.x, height);
            if (row_size == 0) {
                success = false;
                break;
            }

            height += row.GetHeight() + 1;
            if (height > dimensions.y) {
                row.Unplace();
                success = false;
                break;
            }

            rows.push_back(row);
            num_placed_rectangles += row_size;
        }

        if (success)
            return num_placed_rectangles;

        if (dimensions.y > dimensions.x)
            dimensions.x = dimensions.y;
        else {
            if (dimensions.y << 1 > maximum_dimensions)
                return num_placed_rectangles;

            dimensions.y <<= 1;
        }

        for (size_t i = 0; i < rows.size(); i++)
            rows[i].Unplace();

        rows.clear();
        num_placed_rectangles = 0;
    }
}

Rml::Vector<Rml::byte> TextureLayoutTexture::AllocateTexture() {
    Rml::Vector<Rml::byte> texture_data;

    if (dimensions.x > 0 && dimensions.y > 0) {
        texture_data.resize(dimensions.x * dimensions.y * 4, 0);

        for (size_t i = 0; i < rows.size(); ++i)
            rows[i].Allocate(texture_data.data(), dimensions.x * 4);
    }

    return texture_data;
}

} // namespace fabric::ui::font
