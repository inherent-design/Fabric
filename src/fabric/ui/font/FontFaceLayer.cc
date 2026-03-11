#include "FontFaceLayer.hh"
#include "FontFaceHandleHarfBuzz.hh"
#include <string.h>
#include <type_traits>

namespace fabric::ui::font {

FontFaceLayer::FontFaceLayer(const Rml::SharedPtr<const Rml::FontEffect>& _effect) : colour(255, 255, 255) {
    effect = _effect;
    if (effect)
        colour = effect->GetColour();
}

FontFaceLayer::~FontFaceLayer() {}

bool FontFaceLayer::Generate(const FontFaceHandleHarfBuzz* handle, const FontFaceLayer* clone,
                             bool clone_glyph_origins) {
    {
        texture_layout = TextureLayout{};
        character_boxes.clear();
        textures_owned.clear();
        textures_ptr = &textures_owned;
    }

    const FontGlyphMap& glyphs = handle->GetGlyphs();
    const FallbackFontGlyphMap& fallback_glyphs = handle->GetFallbackGlyphs();
    const FallbackFontClusterGlyphsMap& fallback_cluster_glyphs = handle->GetFallbackClusterGlyphs();

    if (clone) {
        character_boxes = clone->character_boxes;

        textures_ptr = clone->textures_ptr;

        if (effect && !clone_glyph_origins) {
            for (auto& pair : glyphs) {
                FontGlyphIndex glyph_index = pair.first;
                const Rml::FontGlyph& glyph = pair.second.bitmap;
                const Rml::Character glyph_character = pair.second.character;

                CloneTextureBox(glyph, glyph_index, glyph_character, false);
            }

            for (auto& pair : fallback_glyphs) {
                const Rml::Character glyph_character = pair.first;
                const Rml::FontGlyph& glyph = pair.second;

                CloneTextureBox(glyph, 0, glyph_character, false);
            }

            for (auto& pair : fallback_cluster_glyphs)
                for (auto& cluster_glyph : pair.second) {
                    const Rml::Character glyph_character = cluster_glyph.glyph_data.character;
                    const Rml::FontGlyph& glyph = cluster_glyph.glyph_data.bitmap;

                    CloneTextureBox(glyph, cluster_glyph.glyph_index, glyph_character, true);
                }
        }
    } else {
        character_boxes.reserve(glyphs.size() + fallback_glyphs.size() + fallback_cluster_glyphs.size());
        for (auto& pair : glyphs) {
            FontGlyphIndex glyph_index = pair.first;
            const Rml::FontGlyph& glyph = pair.second.bitmap;
            Rml::Character glyph_character = pair.second.character;

            CreateTextureLayout(glyph, glyph_index, glyph_character, false);
        }

        for (auto& pair : fallback_glyphs) {
            Rml::Character glyph_character = pair.first;
            const Rml::FontGlyph& glyph = pair.second;

            CreateTextureLayout(glyph, 0, glyph_character, false);
        }

        for (auto& pair : fallback_cluster_glyphs)
            for (auto& cluster_glyph : pair.second) {
                const Rml::Character glyph_character = cluster_glyph.glyph_data.character;
                const Rml::FontGlyph& glyph = cluster_glyph.glyph_data.bitmap;

                CreateTextureLayout(glyph, cluster_glyph.glyph_index, glyph_character, true);
            }

        constexpr int max_texture_dimensions = 1024;

        if (!texture_layout.GenerateLayout(max_texture_dimensions))
            return false;

        for (int i = 0; i < texture_layout.GetNumRectangles(); ++i) {
            TextureLayoutRectangle& rectangle = texture_layout.GetRectangle(i);
            const TextureLayoutTexture& texture = texture_layout.GetTexture(rectangle.GetTextureIndex());
            uint64_t font_glyph_id = rectangle.GetId();
            RMLUI_ASSERT(character_boxes.find(font_glyph_id) != character_boxes.end());
            TextureBox& box = character_boxes[font_glyph_id];

            box.texture_index = rectangle.GetTextureIndex();

            box.texcoords[0].x = float(rectangle.GetPosition().x) / float(texture.GetDimensions().x);
            box.texcoords[0].y = float(rectangle.GetPosition().y) / float(texture.GetDimensions().y);
            box.texcoords[1].x =
                float(rectangle.GetPosition().x + rectangle.GetDimensions().x) / float(texture.GetDimensions().x);
            box.texcoords[1].y =
                float(rectangle.GetPosition().y + rectangle.GetDimensions().y) / float(texture.GetDimensions().y);
        }

        const Rml::FontEffect* effect_ptr = effect.get();
        const int handle_version = handle->GetVersion();

        for (int i = 0; i < texture_layout.GetNumTextures(); ++i) {
            const int texture_id = i;

            Rml::CallbackTextureFunction texture_callback =
                [handle, effect_ptr, texture_id,
                 handle_version](const Rml::CallbackTextureInterface& texture_interface) -> bool {
                Rml::Vector2i dimensions;
                Rml::Vector<Rml::byte> data;
                if (!handle->GenerateLayerTexture(data, dimensions, effect_ptr, texture_id, handle_version) ||
                    data.empty())
                    return false;
                if (!texture_interface.GenerateTexture(data, dimensions))
                    return false;
                return true;
            };

            static_assert(std::is_nothrow_move_constructible<Rml::CallbackTextureSource>::value,
                          "CallbackTextureSource must be nothrow move constructible so that it can be placed in the "
                          "vector below.");

            textures_owned.emplace_back(std::move(texture_callback));
        }
    }

    return true;
}

bool FontFaceLayer::GenerateTexture(Rml::Vector<Rml::byte>& texture_data, Rml::Vector2i& texture_dimensions,
                                    int texture_id, const FontGlyphMaps& glyph_maps) {
    if (texture_id < 0 || texture_id > texture_layout.GetNumTextures())
        return false;

    texture_data = texture_layout.GetTexture(texture_id).AllocateTexture();
    texture_dimensions = texture_layout.GetTexture(texture_id).GetDimensions();

    for (int i = 0; i < texture_layout.GetNumRectangles(); ++i) {
        TextureLayoutRectangle& rectangle = texture_layout.GetRectangle(i);
        uint64_t font_glyph_id = rectangle.GetId();
        RMLUI_ASSERT(character_boxes.find(font_glyph_id) != character_boxes.end());

        TextureBox& box = character_boxes[font_glyph_id];

        if (box.texture_index != texture_id)
            continue;

        const Rml::FontGlyph* glyph = nullptr;
        FontGlyphIndex glyph_index = GetFontGlyphIndexFromID(font_glyph_id);
        Rml::Character glyph_character = GetCharacterCodepointFromID(font_glyph_id);
        bool is_cluster = IsFontGlyphIDPartOfCluster(font_glyph_id);

        RMLUI_ASSERT(glyph_maps.glyphs != nullptr);
        auto it = glyph_maps.glyphs->find(is_cluster ? 0 : glyph_index);
        if (it == glyph_maps.glyphs->end() || glyph_index == 0 || is_cluster) {
            if (is_cluster && glyph_maps.fallback_cluster_glyphs) {
                uint64_t cluster_glyph_lookup_id = GetFallbackFontClusterGlyphLookupID(glyph_index, glyph_character);
                auto cluster_glyph_it = glyph_maps.fallback_cluster_glyphs->find(cluster_glyph_lookup_id);
                if (cluster_glyph_it != glyph_maps.fallback_cluster_glyphs->end())
                    glyph = cluster_glyph_it->second;
            }

            if (!glyph && !is_cluster && glyph_maps.fallback_glyphs) {
                auto fallback_it = glyph_maps.fallback_glyphs->find(glyph_character);
                if (fallback_it != glyph_maps.fallback_glyphs->end())
                    glyph = &fallback_it->second;
            }

            if (!glyph) {
                if (it != glyph_maps.glyphs->end())
                    glyph = &it->second.bitmap;
                else
                    continue;
            }
        } else
            glyph = &it->second.bitmap;

        if (effect == nullptr) {
            if (glyph->bitmap_data) {
                Rml::byte* destination = rectangle.GetTextureData();
                const Rml::byte* source = glyph->bitmap_data;
                const int num_bytes_per_line =
                    glyph->bitmap_dimensions.x * (glyph->color_format == Rml::ColorFormat::RGBA8 ? 4 : 1);

                for (int j = 0; j < glyph->bitmap_dimensions.y; ++j) {
                    switch (glyph->color_format) {
                        case Rml::ColorFormat::A8: {
                            for (int k = 0; k < num_bytes_per_line; ++k)
                                for (int c = 0; c < 4; ++c)
                                    destination[k * 4 + c] = source[k];
                        } break;
                        case Rml::ColorFormat::RGBA8: {
                            memcpy(destination, source, num_bytes_per_line);
                        } break;
                    }

                    destination += rectangle.GetTextureStride();
                    source += num_bytes_per_line;
                }
            }
        } else
            effect->GenerateGlyphTexture(rectangle.GetTextureData(), Rml::Vector2i(box.dimensions),
                                         rectangle.GetTextureStride(), *glyph);
    }

    return true;
}

const Rml::FontEffect* FontFaceLayer::GetFontEffect() const {
    return effect.get();
}

Rml::Texture FontFaceLayer::GetTexture(Rml::RenderManager& render_manager, int index) {
    RMLUI_ASSERT(index >= 0);
    RMLUI_ASSERT(index < GetNumTextures());

    return (*textures_ptr)[index].GetTexture(render_manager);
}

int FontFaceLayer::GetNumTextures() const {
    return (int)textures_ptr->size();
}

Rml::ColourbPremultiplied FontFaceLayer::GetColour(float opacity) const {
    return colour.ToPremultiplied(opacity);
}

uint64_t FontFaceLayer::CreateFontGlyphID(const FontGlyphIndex glyph_index, const Rml::Character character_code,
                                          bool is_cluster) const {
    uint64_t font_glyph_id =
        (static_cast<uint64_t>(glyph_index) << (sizeof(Rml::Character) * 8)) | static_cast<uint64_t>(character_code);

    if (is_cluster)
        font_glyph_id |= K_FONT_GLYPH_ID_CLUSTER_BIT_MASK;
    else
        font_glyph_id &= ~K_FONT_GLYPH_ID_CLUSTER_BIT_MASK;

    return font_glyph_id;
}

FontGlyphIndex FontFaceLayer::GetFontGlyphIndexFromID(const uint64_t glyph_id) const {
    return static_cast<FontGlyphIndex>(glyph_id >> (sizeof(Rml::Character) * 8));
}

Rml::Character FontFaceLayer::GetCharacterCodepointFromID(const uint64_t glyph_id) const {
    uint64_t character_codepoint = glyph_id & static_cast<std::underlying_type_t<Rml::Character>>(-1);
    character_codepoint &= ~K_FONT_GLYPH_ID_CLUSTER_BIT_MASK;

    return static_cast<Rml::Character>(character_codepoint);
}

bool FontFaceLayer::IsFontGlyphIDPartOfCluster(const uint64_t glyph_id) const {
    return glyph_id & K_FONT_GLYPH_ID_CLUSTER_BIT_MASK;
}

void FontFaceLayer::CreateTextureLayout(const Rml::FontGlyph& glyph, FontGlyphIndex glyph_index,
                                        Rml::Character glyph_character, bool is_cluster) {
    Rml::Vector2i glyph_origin(0, 0);
    Rml::Vector2i glyph_dimensions = glyph.bitmap_dimensions;

    if (effect) {
        if (!effect->GetGlyphMetrics(glyph_origin, glyph_dimensions, glyph))
            return;
    }

    TextureBox box;
    box.origin = Rml::Vector2f(float(glyph_origin.x + glyph.bearing.x), float(glyph_origin.y - glyph.bearing.y));
    box.dimensions = Rml::Vector2f(glyph_dimensions);

    RMLUI_ASSERT(box.dimensions.x >= 0 && box.dimensions.y >= 0);

    uint64_t font_glyph_id = CreateFontGlyphID(glyph_index, glyph_character, is_cluster);
    character_boxes[font_glyph_id] = box;

    texture_layout.AddRectangle(font_glyph_id, glyph_dimensions);
}

void FontFaceLayer::CloneTextureBox(const Rml::FontGlyph& glyph, FontGlyphIndex glyph_index,
                                    Rml::Character glyph_character, bool is_cluster) {
    auto it = character_boxes.find(CreateFontGlyphID(glyph_index, glyph_character, is_cluster));
    if (it == character_boxes.end()) {
        return;
    }

    TextureBox& box = it->second;

    Rml::Vector2i glyph_origin = Rml::Vector2i(box.origin);
    Rml::Vector2i glyph_dimensions = Rml::Vector2i(box.dimensions);

    if (effect->GetGlyphMetrics(glyph_origin, glyph_dimensions, glyph))
        box.origin = Rml::Vector2f(glyph_origin);
    else
        box.texture_index = -1;
}

} // namespace fabric::ui::font
