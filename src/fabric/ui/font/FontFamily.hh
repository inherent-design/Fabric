#pragma once

#include "FontTypes.hh"
#include <RmlUi/Core.h>

namespace fabric::ui::font {

class FontFace;
class FontFaceHandleHarfBuzz;

class FontFamily {
  public:
    FontFamily(const Rml::String& name);
    ~FontFamily();

    FontFaceHandleHarfBuzz* GetFaceHandle(Rml::Style::FontStyle style, Rml::Style::FontWeight weight, int size);

    FontFace* AddFace(FontFaceHandleFreetype ft_face, Rml::Style::FontStyle style, Rml::Style::FontWeight weight,
                      Rml::UniquePtr<Rml::byte[]> face_memory);

    void ReleaseFontResources();

  protected:
    Rml::String name;

    struct FontFaceEntry {
        Rml::UniquePtr<FontFace> face;
        Rml::UniquePtr<Rml::byte[]> face_memory;
    };

    using FontFaceList = Rml::Vector<FontFaceEntry>;
    FontFaceList font_faces;
};

} // namespace fabric::ui::font
