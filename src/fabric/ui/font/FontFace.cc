#include "FontFace.hh"
#include "FontEngineDefault/FreeTypeInterface.h"

namespace fabric::ui::font {

FontFace::FontFace(FontFaceHandleFreetype _face, Rml::Style::FontStyle _style, Rml::Style::FontWeight _weight) {
    style = _style;
    weight = _weight;
    face = _face;
}

FontFace::~FontFace() {
    if (face)
        Rml::FreeType::ReleaseFace(face);
}

Rml::Style::FontStyle FontFace::GetStyle() const {
    return style;
}

Rml::Style::FontWeight FontFace::GetWeight() const {
    return weight;
}

FontFaceHandleHarfBuzz* FontFace::GetHandle(int size, bool load_default_glyphs) {
    auto it = handles.find(size);
    if (it != handles.end())
        return it->second.get();

    if (!face) {
        Rml::Log::Message(Rml::Log::LT_WARNING, "Font face has been released, unable to generate new handle.");
        return nullptr;
    }

    auto handle = Rml::MakeUnique<FontFaceHandleHarfBuzz>();
    if (!handle->Initialize(face, size, load_default_glyphs)) {
        handles[size] = nullptr;
        return nullptr;
    }

    FontFaceHandleHarfBuzz* result = handle.get();

    handles[size] = std::move(handle);

    return result;
}

void FontFace::ReleaseFontResources() {
    HandleMap().swap(handles);
}

} // namespace fabric::ui::font
