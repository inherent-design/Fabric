#include "fabric/ui/font/FontEngineInterfaceHarfBuzz.hh"
#include "FontFaceHandleHarfBuzz.hh"
#include "FontProvider.hh"
#include <RmlUi/Core.h>

namespace fabric::ui::font {

void FontEngineInterfaceHarfBuzz::Initialize() {
    FontProvider::Initialise();
}
void FontEngineInterfaceHarfBuzz::Shutdown() {
    FontProvider::Shutdown();
}

bool FontEngineInterfaceHarfBuzz::LoadFontFace(const Rml::String& file_name, int face_index, bool fallback_face,
                                               Rml::Style::FontWeight weight) {
    return FontProvider::LoadFontFace(file_name, face_index, fallback_face, weight);
}

bool FontEngineInterfaceHarfBuzz::LoadFontFace(Rml::Span<const Rml::byte> data, int face_index,
                                               const Rml::String& font_family, Rml::Style::FontStyle style,
                                               Rml::Style::FontWeight weight, bool fallback_face) {
    return FontProvider::LoadFontFace(data, face_index, font_family, style, weight, fallback_face);
}

Rml::FontFaceHandle FontEngineInterfaceHarfBuzz::GetFontFaceHandle(const Rml::String& family,
                                                                   Rml::Style::FontStyle style,
                                                                   Rml::Style::FontWeight weight, int size) {
    auto handle = FontProvider::GetFontFaceHandle(family, style, weight, size);
    return reinterpret_cast<Rml::FontFaceHandle>(handle);
}

Rml::FontEffectsHandle FontEngineInterfaceHarfBuzz::PrepareFontEffects(Rml::FontFaceHandle handle,
                                                                       const Rml::FontEffectList& font_effects) {
    auto handle_harfbuzz = reinterpret_cast<FontFaceHandleHarfBuzz*>(handle);
    return (Rml::FontEffectsHandle)handle_harfbuzz->GenerateLayerConfiguration(font_effects);
}

const Rml::FontMetrics& FontEngineInterfaceHarfBuzz::GetFontMetrics(Rml::FontFaceHandle handle) {
    auto handle_harfbuzz = reinterpret_cast<FontFaceHandleHarfBuzz*>(handle);
    return handle_harfbuzz->GetFontMetrics();
}

int FontEngineInterfaceHarfBuzz::GetStringWidth(Rml::FontFaceHandle handle, Rml::StringView string,
                                                const Rml::TextShapingContext& text_shaping_context,
                                                Rml::Character prior_character) {
    auto handle_harfbuzz = reinterpret_cast<FontFaceHandleHarfBuzz*>(handle);
    return handle_harfbuzz->GetStringWidth(string, text_shaping_context, registered_languages, prior_character);
}

int FontEngineInterfaceHarfBuzz::GenerateString(Rml::RenderManager& render_manager, Rml::FontFaceHandle handle,
                                                Rml::FontEffectsHandle font_effects_handle, Rml::StringView string,
                                                Rml::Vector2f position, Rml::ColourbPremultiplied colour, float opacity,
                                                const Rml::TextShapingContext& text_shaping_context,
                                                Rml::TexturedMeshList& mesh_list) {
    auto handle_harfbuzz = reinterpret_cast<FontFaceHandleHarfBuzz*>(handle);
    return handle_harfbuzz->GenerateString(render_manager, mesh_list, string, position, colour, opacity,
                                           text_shaping_context, registered_languages, (int)font_effects_handle);
}

int FontEngineInterfaceHarfBuzz::GetVersion(Rml::FontFaceHandle handle) {
    auto handle_harfbuzz = reinterpret_cast<FontFaceHandleHarfBuzz*>(handle);
    return handle_harfbuzz->GetVersion();
}

void FontEngineInterfaceHarfBuzz::ReleaseFontResources() {
    FontProvider::ReleaseFontResources();
}

void FontEngineInterfaceHarfBuzz::RegisterLanguage(const Rml::String& language_bcp47_code,
                                                   const Rml::String& script_iso15924_code,
                                                   const TextFlowDirection text_flow_direction) {
    registered_languages[language_bcp47_code] = LanguageData{script_iso15924_code, text_flow_direction};
}

} // namespace fabric::ui::font
