#pragma once

#include "fabric/ui/font/LanguageData.hh"
#include "FontFaceLayer.hh"
#include "FontGlyph.hh"
#include "FontTypes.hh"
#include <hb.h>
#include <RmlUi/Core.h>

namespace fabric::ui::font {

class FontFaceHandleHarfBuzz : public Rml::NonCopyMoveable {
  public:
    FontFaceHandleHarfBuzz();
    ~FontFaceHandleHarfBuzz();

    bool Initialize(FontFaceHandleFreetype face, int font_size, bool load_default_glyphs);

    const Rml::FontMetrics& GetFontMetrics() const;

    const FontGlyphMap& GetGlyphs() const;
    const FallbackFontGlyphMap& GetFallbackGlyphs() const;
    const FallbackFontClusterGlyphsMap& GetFallbackClusterGlyphs() const;

    int GetStringWidth(Rml::StringView string, const Rml::TextShapingContext& text_shaping_context,
                       const LanguageDataMap& registered_languages,
                       Rml::Character prior_character = Rml::Character::Null);

    int GenerateLayerConfiguration(const Rml::FontEffectList& font_effects);

    bool GenerateLayerTexture(Rml::Vector<Rml::byte>& texture_data, Rml::Vector2i& texture_dimensions,
                              const Rml::FontEffect* font_effect, int texture_id, int handle_version) const;

    int GenerateString(Rml::RenderManager& render_manager, Rml::TexturedMeshList& mesh_list, Rml::StringView string,
                       Rml::Vector2f position, Rml::ColourbPremultiplied colour, float opacity,
                       const Rml::TextShapingContext& text_shaping_context, const LanguageDataMap& registered_languages,
                       int layer_configuration = 0);

    int GetVersion() const;

  private:
    bool AppendGlyph(FontGlyphIndex glyph_index, Rml::Character character);
    bool AppendFallbackGlyph(Rml::Character& character);
    const Rml::FontGlyph* GetOrAppendGlyph(FontGlyphIndex glyph_index, Rml::Character& character,
                                           bool look_in_fallback_fonts = true);
    const Rml::FontGlyph* GetOrAppendFallbackGlyph(Rml::Character& character);

    bool AppendFallbackClusterGlyphs(Rml::StringView cluster, const Rml::TextShapingContext& text_shaping_context,
                                     const LanguageDataMap& registered_languages,
                                     Rml::Span<hb_feature_t> text_shaping_features);

    const Rml::Vector<FontClusterGlyphData>*
    GetOrAppendFallbackClusterGlyphs(Rml::StringView cluster, const Rml::TextShapingContext& text_shaping_context,
                                     const LanguageDataMap& registered_languages,
                                     Rml::Span<hb_feature_t> text_shaping_features);

    bool UpdateLayersOnDirty();

    FontFaceLayer* GetOrCreateLayer(const Rml::SharedPtr<const Rml::FontEffect>& font_effect);

    bool GenerateLayer(FontFaceLayer* layer);

    void ConfigureTextShapingBuffer(hb_buffer_t* shaping_buffer, Rml::StringView string,
                                    const Rml::TextShapingContext& text_shaping_context,
                                    const LanguageDataMap& registered_languages,
                                    TextFlowDirection* determined_text_direction) const;

    Rml::StringView GetCurrentClusterString(const hb_glyph_info_t* glyph_info, int glyph_count, int glyph_index,
                                            Rml::Character first_character, Rml::StringView string,
                                            int& cluster_codepoint_count) const;

    FontGlyphMap glyphs;
    FallbackFontGlyphMap fallback_glyphs;

    FallbackFontClusterGlyphsMap fallback_cluster_glyphs;
    FallbackFontClusterGlyphLookupMap fallback_cluster_glyphs_lookup;

    struct EffectLayerPair {
        const Rml::FontEffect* font_effect;
        Rml::UniquePtr<FontFaceLayer> layer;
    };
    using FontLayerMap = Rml::Vector<EffectLayerPair>;
    using FontLayerCache = Rml::SmallUnorderedMap<size_t, FontFaceLayer*>;
    using LayerConfiguration = Rml::Vector<FontFaceLayer*>;
    using LayerConfigurationList = Rml::Vector<LayerConfiguration>;

    FontFaceLayer* base_layer;
    FontLayerMap layers;
    FontLayerCache layer_cache;

    bool is_layers_dirty = false;
    int version = 0;

    LayerConfigurationList layer_configurations;

    Rml::FontMetrics metrics;

    FontFaceHandleFreetype ft_face;
    hb_font_t* hb_font;
};

} // namespace fabric::ui::font
