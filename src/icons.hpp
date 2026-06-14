#pragma once

#include "imgui.h"

#include "status.hpp"

namespace rni {

// Map a composite EdgeStatus to its palette color (used by icons + edge tint).
ImU32 status_color(EdgeStatus s);

// Draw the status marker centered at `center` with radius `r` on `dl`.
// Vector primitives only (crisp under graph zoom):
//   Ok          -> green checkmark (two strokes)
//   QosMismatch -> filled amber circle + "?" glyph
//   TypeMismatch/Dead -> red cross (two strokes)
//   Unknown     -> grey circle outline
// These are the always-available fallback; FontAwesome is toolbar-only polish.
void draw_status_icon(ImDrawList * dl, EdgeStatus s, ImVec2 center, float r);

// Load the base UI font: PT Sans (shipped in the package share dir) for the
// interface, with DejaVuSans merged over it for the arrow/dash/bullet glyphs PT
// Sans lacks. Falls back to whole-font DejaVuSans, then ImGui's built-in bitmap
// default. Call once at startup before merging FontAwesome.
void load_ui_font(float size_px);

// Attempt to merge third_party/fonts/fa-solid-900.ttf into the ImGui atlas for
// richer toolbar/legend glyphs (ICON_FA_*). No-op (returns false) if the font is
// absent — the app still runs with vector status icons. Call once at startup,
// after the default font is added and before the atlas is built.
bool try_load_fontawesome(float size_px);

// True if try_load_fontawesome() succeeded this session.
bool fontawesome_loaded();

}  // namespace rni
