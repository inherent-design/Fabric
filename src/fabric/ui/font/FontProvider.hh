#pragma once

#include "FontTypes.hh"
#include <RmlUi/Core.h>

namespace fabric::ui::font {

class FontFace;
class FontFamily;
class FontFaceHandleHarfBuzz;

class FontProvider {
  public:
    static bool Initialise();
    static void Shutdown();

    static FontFaceHandleHarfBuzz* GetFontFaceHandle(const Rml::String& family, Rml::Style::FontStyle style,
                                                     Rml::Style::FontWeight weight, int size);

    static bool LoadFontFace(const Rml::String& file_name, int face_index, bool fallback_face,
                             Rml::Style::FontWeight weight = Rml::Style::FontWeight::Auto);

    static bool LoadFontFace(Rml::Span<const Rml::byte> data, int face_index, const Rml::String& font_family,
                             Rml::Style::FontStyle style, Rml::Style::FontWeight weight, bool fallback_face);

    static int CountFallbackFontFaces();

    static FontFaceHandleHarfBuzz* GetFallbackFontFace(int index, int font_size);

    static void ReleaseFontResources();

  private:
    FontProvider();
    ~FontProvider();

    static FontProvider& Get();

    bool LoadFontFace(Rml::Span<const Rml::byte> data, int face_index, bool fallback_face,
                      Rml::UniquePtr<Rml::byte[]> face_memory, const Rml::String& source, Rml::String font_family,
                      Rml::Style::FontStyle style, Rml::Style::FontWeight weight);

    bool AddFace(FontFaceHandleFreetype face, const Rml::String& family, Rml::Style::FontStyle style,
                 Rml::Style::FontWeight weight, bool fallback_face, Rml::UniquePtr<Rml::byte[]> face_memory);

    using FontFaceList = Rml::Vector<FontFace*>;
    using FontFamilyMap = Rml::UnorderedMap<Rml::String, Rml::UniquePtr<FontFamily>>;

    FontFamilyMap font_families;
    FontFaceList fallback_font_faces;
};

} // namespace fabric::ui::font
