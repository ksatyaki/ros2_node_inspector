#pragma once

#include "imgui.h"

// Muted dark theme — Tokyo-Night / Foxglove family. Deliberately NOT RViz's
// saturated primaries. Pinned hex so "nice colors" is unambiguous. Applied via
// ImGui::StyleColorsDark() then overriding the entries in apply_palette().
namespace pal {

constexpr ImU32 bg       = IM_COL32(0x1A, 0x1B, 0x26, 0xFF); // window background, deep navy-charcoal
constexpr ImU32 panel    = IM_COL32(0x24, 0x28, 0x3B, 0xFF); // node boxes, table rows
constexpr ImU32 edge     = IM_COL32(0x3B, 0x42, 0x61, 0xFF); // idle topic line / borders
constexpr ImU32 text     = IM_COL32(0xC0, 0xCA, 0xF5, 0xFF); // soft lavender (no pure white)
constexpr ImU32 text_dim = IM_COL32(0x7A, 0x82, 0xA8, 0xFF); // secondary labels, types
constexpr ImU32 accent   = IM_COL32(0x7A, 0xA2, 0xF7, 0xFF); // selected node, focus ring, links
constexpr ImU32 ok       = IM_COL32(0x6B, 0xC4, 0x8A, 0xFF); // tick   (muted sea green)
constexpr ImU32 warn     = IM_COL32(0xE0, 0xAF, 0x68, 0xFF); // ?      (soft amber)
constexpr ImU32 error    = IM_COL32(0xE0, 0x6C, 0x75, 0xFF); // cross  (soft coral, not neon red)
constexpr ImU32 unknown  = IM_COL32(0x5C, 0x63, 0x70, 0xFF); // grey   (probe pending)

inline ImVec4 to_vec4(ImU32 c)
{
  return ImColor(c).Value;
}

// StyleColorsDark() then override the palette entries that matter.
inline void apply_palette()
{
  ImGui::StyleColorsDark();
  ImGuiStyle & s = ImGui::GetStyle();
  s.WindowRounding = 6.0f;
  s.FrameRounding = 4.0f;
  s.GrabRounding = 4.0f;
  s.Colors[ImGuiCol_WindowBg]        = to_vec4(bg);
  s.Colors[ImGuiCol_ChildBg]         = to_vec4(bg);
  s.Colors[ImGuiCol_PopupBg]         = to_vec4(panel);
  s.Colors[ImGuiCol_FrameBg]         = to_vec4(panel);
  s.Colors[ImGuiCol_FrameBgHovered]  = to_vec4(edge);
  s.Colors[ImGuiCol_FrameBgActive]   = to_vec4(edge);
  s.Colors[ImGuiCol_TitleBg]         = to_vec4(panel);
  s.Colors[ImGuiCol_TitleBgActive]   = to_vec4(panel);
  s.Colors[ImGuiCol_Header]          = to_vec4(edge);
  s.Colors[ImGuiCol_HeaderHovered]   = to_vec4(accent);
  s.Colors[ImGuiCol_HeaderActive]    = to_vec4(accent);
  s.Colors[ImGuiCol_Button]          = to_vec4(panel);
  s.Colors[ImGuiCol_ButtonHovered]   = to_vec4(edge);
  s.Colors[ImGuiCol_ButtonActive]    = to_vec4(accent);
  s.Colors[ImGuiCol_Tab]             = to_vec4(panel);
  s.Colors[ImGuiCol_TabHovered]      = to_vec4(accent);
  s.Colors[ImGuiCol_TabActive]       = to_vec4(edge);
  s.Colors[ImGuiCol_TabUnfocused]    = to_vec4(panel);
  s.Colors[ImGuiCol_TabUnfocusedActive] = to_vec4(edge);
  s.Colors[ImGuiCol_Text]            = to_vec4(text);
  s.Colors[ImGuiCol_TextDisabled]    = to_vec4(text_dim);
  s.Colors[ImGuiCol_Border]          = to_vec4(edge);
  s.Colors[ImGuiCol_TableHeaderBg]   = to_vec4(panel);
  s.Colors[ImGuiCol_TableRowBg]      = to_vec4(bg);
  s.Colors[ImGuiCol_TableRowBgAlt]   = to_vec4(panel);
  s.Colors[ImGuiCol_TableBorderLight]= to_vec4(edge);
  s.Colors[ImGuiCol_TableBorderStrong]= to_vec4(edge);
  s.Colors[ImGuiCol_CheckMark]       = to_vec4(accent);
  s.Colors[ImGuiCol_SliderGrab]      = to_vec4(accent);
}

// Map an EdgeStatus color (forward-declared enum kept out of this header to
// avoid a cycle; callers pass the ImU32 directly from status_color()).

}  // namespace pal
