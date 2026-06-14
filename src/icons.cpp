#include "icons.hpp"

#include <cstdio>
#include <string>

#include "ament_index_cpp/get_package_share_directory.hpp"

#include "palette.hpp"

namespace rni {
namespace {
bool g_fa_loaded = false;
}

ImU32 status_color(EdgeStatus s)
{
  switch (s) {
    case EdgeStatus::Ok:           return pal::ok;
    case EdgeStatus::QosMismatch:  return pal::warn;
    case EdgeStatus::TypeMismatch: return pal::error;
    case EdgeStatus::Dead:         return pal::error;
    case EdgeStatus::Unknown:      return pal::unknown;
  }
  return pal::unknown;
}

void draw_status_icon(ImDrawList * dl, EdgeStatus s, ImVec2 c, float r)
{
  const ImU32 col = status_color(s);
  const float th = (r * 0.34f) < 1.5f ? 1.5f : r * 0.34f;

  switch (s) {
    case EdgeStatus::Ok: {
      // Checkmark: down-stroke then up-stroke.
      dl->AddLine(ImVec2(c.x - 0.55f * r, c.y + 0.05f * r),
                  ImVec2(c.x - 0.10f * r, c.y + 0.50f * r), col, th);
      dl->AddLine(ImVec2(c.x - 0.10f * r, c.y + 0.50f * r),
                  ImVec2(c.x + 0.60f * r, c.y - 0.45f * r), col, th);
      break;
    }
    case EdgeStatus::TypeMismatch:
    case EdgeStatus::Dead: {
      // Cross: two diagonals.
      dl->AddLine(ImVec2(c.x - 0.50f * r, c.y - 0.50f * r),
                  ImVec2(c.x + 0.50f * r, c.y + 0.50f * r), col, th);
      dl->AddLine(ImVec2(c.x - 0.50f * r, c.y + 0.50f * r),
                  ImVec2(c.x + 0.50f * r, c.y - 0.50f * r), col, th);
      break;
    }
    case EdgeStatus::QosMismatch: {
      // Filled amber disc + a "?" glyph from the default font.
      dl->AddCircleFilled(c, r, col);
      ImFont * font = ImGui::GetFont();
      const float fsz = r * 1.6f;
      const ImVec2 sz = font->CalcTextSizeA(fsz, FLT_MAX, 0.0f, "?");
      dl->AddText(font, fsz, ImVec2(c.x - sz.x * 0.5f, c.y - sz.y * 0.5f),
                  pal::bg, "?");
      break;
    }
    case EdgeStatus::Unknown: {
      dl->AddCircle(c, r * 0.85f, col, 0, th);
      break;
    }
  }
}

void load_ui_font(float size_px)
{
  ImGuiIO & io = ImGui::GetIO();

  // Glyph ranges: Latin + the few symbol blocks our labels/popups use
  // (general punctuation dashes, arrows, geometric-shape bullet). Static so the
  // pointer stays valid until the atlas is built.
  static const ImWchar ranges[] = {
    0x0020, 0x00FF,  // Basic Latin + Latin-1 Supplement
    0x2010, 0x2027,  // dashes / quotes (– — …)
    0x2190, 0x21FF,  // arrows (← →)
    0x25A0, 0x25FF,  // geometric shapes (● bullet)
    0,
  };

  const char * candidates[] = {
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/dejavu/DejaVuSans.ttf",
  };
  for (const char * path : candidates) {
    if (FILE * f = std::fopen(path, "rb")) {
      std::fclose(f);
      if (io.Fonts->AddFontFromFileTTF(path, size_px, nullptr, ranges)) {
        return;
      }
    }
  }
  io.Fonts->AddFontDefault();  // built-in bitmap fallback (ASCII only)
}

bool try_load_fontawesome(float size_px)
{
  std::string ttf;
  try {
    ttf = ament_index_cpp::get_package_share_directory("ros2_node_inspector") +
          "/fonts/fa-solid-900.ttf";
  } catch (...) {
    return false;
  }

  ImGuiIO & io = ImGui::GetIO();
  // Merge FA into the existing (default) font so ICON_FA_* render inline.
  static const ImWchar ranges[] = {0xe005, 0xf8ff, 0};  // FA solid block
  ImFontConfig cfg;
  cfg.MergeMode = true;
  cfg.PixelSnapH = true;
  cfg.GlyphMinAdvanceX = size_px;  // monospace icons
  ImFont * f = io.Fonts->AddFontFromFileTTF(ttf.c_str(), size_px, &cfg, ranges);
  g_fa_loaded = (f != nullptr);
  return g_fa_loaded;
}

bool fontawesome_loaded()
{
  return g_fa_loaded;
}

}  // namespace rni
