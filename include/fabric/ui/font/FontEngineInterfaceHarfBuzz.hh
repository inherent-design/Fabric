#pragma once

#include "LanguageData.hh"
#include <RmlUi/Core/FontEngineInterface.h>

namespace fabric::ui::font {

class FontEngineInterfaceHarfBuzz : public Rml::FontEngineInterface {
  public:
    void Initialize() override;
    void Shutdown() override;

    bool LoadFontFace(const Rml::String& file_name, int face_index, bool fallback_face,
                      Rml::Style::FontWeight weight) override;

    bool LoadFontFace(Rml::Span<const Rml::byte> data, int face_index, const Rml::String& font_family,
                      Rml::Style::FontStyle style, Rml::Style::FontWeight weight, bool fallback_face) override;

    Rml::FontFaceHandle GetFontFaceHandle(const Rml::String& family, Rml::Style::FontStyle style,
                                          Rml::Style::FontWeight weight, int size) override;

    Rml::FontEffectsHandle PrepareFontEffects(Rml::FontFaceHandle handle,
                                              const Rml::FontEffectList& font_effects) override;

    const Rml::FontMetrics& GetFontMetrics(Rml::FontFaceHandle handle) override;

    int GetStringWidth(Rml::FontFaceHandle handle, Rml::StringView string,
                       const Rml::TextShapingContext& text_shaping_context, Rml::Character prior_character) override;

    int GenerateString(Rml::RenderManager& render_manager, Rml::FontFaceHandle face_handle,
                       Rml::FontEffectsHandle effects_handle, Rml::StringView string, Rml::Vector2f position,
                       Rml::ColourbPremultiplied colour, float opacity,
                       const Rml::TextShapingContext& text_shaping_context, Rml::TexturedMeshList& mesh_list) override;

    int GetVersion(Rml::FontFaceHandle handle) override;

    void ReleaseFontResources() override;

    void RegisterLanguage(const Rml::String& language_bcp47_code, const Rml::String& script_iso15924_code,
                          const TextFlowDirection text_flow_direction);

  private:
    LanguageDataMap registered_languages;
};

} // namespace fabric::ui::font
