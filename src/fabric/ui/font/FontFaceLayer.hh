#pragma once

#include "FontGlyph.hh"
#include "TextureLayout.hh"
#include "TextureLayoutRectangle.hh"
#include <RmlUi/Core.h>

namespace fabric::ui::font {

class FontFaceHandleHarfBuzz;

class FontFaceLayer {
  public:
    FontFaceLayer(const Rml::SharedPtr<const Rml::FontEffect>& _effect);
    ~FontFaceLayer();

    bool Generate(const FontFaceHandleHarfBuzz* handle, const FontFaceLayer* clone = nullptr,
                  bool clone_glyph_origins = false);

    bool GenerateTexture(Rml::Vector<Rml::byte>& texture_data, Rml::Vector2i& texture_dimensions, int texture_id,
                         const FontGlyphMaps& glyph_maps);

    inline void GenerateGeometry(Rml::TexturedMesh* mesh_list, const FontGlyphIndex glyph_index,
                                 const Rml::Character character_code, bool is_cluster, const Rml::Vector2f position,
                                 const Rml::ColourbPremultiplied colour) const {
        auto it = character_boxes.find(CreateFontGlyphID(glyph_index, character_code, is_cluster));
        if (it == character_boxes.end())
            return;

        const TextureBox& box = it->second;

        if (box.texture_index < 0)
            return;

        Rml::Mesh& mesh = mesh_list[box.texture_index].mesh;
        Rml::MeshUtilities::GenerateQuad(mesh, (position + box.origin).Round(), box.dimensions, colour,
                                         box.texcoords[0], box.texcoords[1]);
    }

    const Rml::FontEffect* GetFontEffect() const;

    Rml::Texture GetTexture(Rml::RenderManager& render_manager, int index);
    int GetNumTextures() const;

    Rml::ColourbPremultiplied GetColour(float opacity) const;

  private:
    uint64_t CreateFontGlyphID(const FontGlyphIndex glyph_index, const Rml::Character character_code,
                               bool is_cluster) const;

    FontGlyphIndex GetFontGlyphIndexFromID(const uint64_t glyph_id) const;

    Rml::Character GetCharacterCodepointFromID(const uint64_t glyph_id) const;

    bool IsFontGlyphIDPartOfCluster(const uint64_t glyph_id) const;

    void CreateTextureLayout(const Rml::FontGlyph& glyph, FontGlyphIndex glyph_index, Rml::Character glyph_character,
                             bool is_cluster);

    void CloneTextureBox(const Rml::FontGlyph& glyph, FontGlyphIndex glyph_index, Rml::Character glyph_character,
                         bool is_cluster);

    struct TextureBox {
        Rml::Vector2f origin;
        Rml::Vector2f dimensions;
        Rml::Vector2f texcoords[2];

        int texture_index = -1;
    };

    using CharacterMap = Rml::UnorderedMap<uint64_t, TextureBox>;
    using TextureList = Rml::Vector<Rml::CallbackTextureSource>;

    static constexpr uint64_t K_FONT_GLYPH_ID_CLUSTER_BIT_MASK = 1ull << 31ull;

    Rml::SharedPtr<const Rml::FontEffect> effect;

    TextureList textures_owned;
    TextureList* textures_ptr = &textures_owned;

    TextureLayout texture_layout;
    CharacterMap character_boxes;
    Rml::Colourb colour;
};

} // namespace fabric::ui::font
