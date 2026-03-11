#pragma once

#include "FontGlyph.hh"
#include "FontTypes.hh"
#include <RmlUi/Core.h>

namespace fabric::ui::font {
namespace FreeType {

bool InitialiseFaceHandle(FontFaceHandleFreetype face, int font_size, FontGlyphMap& glyphs, Rml::FontMetrics& metrics,
                          bool load_default_glyphs);

bool AppendGlyph(FontFaceHandleFreetype face, int font_size, FontGlyphIndex glyph_index, Rml::Character character,
                 FontGlyphMap& glyphs);

FontGlyphIndex GetGlyphIndexFromCharacter(FontFaceHandleFreetype face, Rml::Character character);

} // namespace FreeType
} // namespace fabric::ui::font
