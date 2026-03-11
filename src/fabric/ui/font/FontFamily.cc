#include "FontFamily.hh"
#include "FontFace.hh"
#include "FontFaceHandleHarfBuzz.hh"
#include <limits.h>

namespace fabric::ui::font {

FontFamily::FontFamily(const Rml::String& name) : name(name) {}

FontFamily::~FontFamily() {
    for (FontFaceEntry& entry : font_faces)
        entry.face.reset();
}

FontFaceHandleHarfBuzz* FontFamily::GetFaceHandle(Rml::Style::FontStyle style, Rml::Style::FontWeight weight,
                                                  int size) {
    int best_dist = INT_MAX;
    FontFace* matching_face = nullptr;
    for (size_t i = 0; i < font_faces.size(); i++) {
        FontFace* face = font_faces[i].face.get();

        if (face->GetStyle() == style) {
            const int dist = Rml::Math::Absolute((int)face->GetWeight() - (int)weight);
            if (dist == 0) {
                matching_face = face;
                break;
            } else if (dist < best_dist) {
                matching_face = face;
                best_dist = dist;
            }
        }
    }

    if (!matching_face)
        return nullptr;

    return matching_face->GetHandle(size, true);
}

FontFace* FontFamily::AddFace(FontFaceHandleFreetype ft_face, Rml::Style::FontStyle style,
                              Rml::Style::FontWeight weight, Rml::UniquePtr<Rml::byte[]> face_memory) {
    auto face = Rml::MakeUnique<FontFace>(ft_face, style, weight);
    FontFace* result = face.get();

    font_faces.push_back(FontFaceEntry{std::move(face), std::move(face_memory)});

    return result;
}

void FontFamily::ReleaseFontResources() {
    for (auto& entry : font_faces)
        entry.face->ReleaseFontResources();
}

} // namespace fabric::ui::font
