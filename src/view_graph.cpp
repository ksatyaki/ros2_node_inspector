#include "view_graph.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "palette.hpp"

namespace rni {
namespace {

// World-space layout constants (pre-zoom).
constexpr float kBoxW = 180.0f;
constexpr float kBoxH = 34.0f;
constexpr float kColX = 340.0f;   // center -> side column, in world units
constexpr float kRowY = 70.0f;    // vertical spacing between peers on a side
constexpr float kIconR = 9.0f;    // status icon radius (world units)
constexpr float kLabelHideZoom = 0.55f;  // below this, draw icon only

// One drawn peer: its box center in world space + the connection it represents.
struct Slot {
  ImVec2 world;            // box center
  const Connection * conn;
};

// Lay peers out in a vertical column centered on world-y 0.
void layout_column(const std::vector<Connection> & edges, float x,
                   std::vector<Slot> & out)
{
  const int n = static_cast<int>(edges.size());
  for (int i = 0; i < n; ++i) {
    const float y = (static_cast<float>(i) - (n - 1) * 0.5f) * kRowY;
    out.push_back({ImVec2(x, y), &edges[i]});
  }
}

ImU32 with_alpha(ImU32 c, float a)
{
  ImVec4 v = ImColor(c).Value;
  v.w *= a;
  return ImColor(v);
}

}  // namespace

std::string render_graph_view(const NodeView & view, GraphState & gs,
                              StatusPopup & popup)
{
  ImGuiIO & io = ImGui::GetIO();
  ImDrawList * dl = ImGui::GetWindowDrawList();

  const ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
  ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
  canvas_sz.x = std::max(canvas_sz.x, 80.0f);
  canvas_sz.y = std::max(canvas_sz.y, 80.0f);
  const ImVec2 canvas_p1(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);
  const ImVec2 center(canvas_p0.x + canvas_sz.x * 0.5f,
                      canvas_p0.y + canvas_sz.y * 0.5f);

  dl->AddRectFilled(canvas_p0, canvas_p1, pal::bg, 4.0f);
  dl->AddRect(canvas_p0, canvas_p1, pal::edge, 4.0f);
  dl->PushClipRect(canvas_p0, canvas_p1, true);

  // Reset pan once when a fresh node is selected.
  if (!gs.centered) {
    gs.pan = ImVec2(0, 0);
    gs.centered = true;
  }

  // Canvas interaction surface (captures pan-drag, wheel, hover).
  ImGui::SetCursorScreenPos(canvas_p0);
  ImGui::InvisibleButton("##graph_canvas", canvas_sz,
                         ImGuiButtonFlags_MouseButtonLeft);
  const bool hovered = ImGui::IsItemHovered();
  const bool active = ImGui::IsItemActive();

  if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    gs.pan.x += io.MouseDelta.x;
    gs.pan.y += io.MouseDelta.y;
  }

  // Zoom about the cursor: keep the world point under the mouse fixed.
  if (hovered && io.MouseWheel != 0.0f) {
    const float old_zoom = gs.zoom;
    gs.zoom = std::clamp(gs.zoom * std::pow(1.1f, io.MouseWheel), 0.25f, 4.0f);
    const ImVec2 m = io.MousePos;
    // world = (m - center - pan) / old_zoom ; keep it fixed at new zoom.
    const float wx = (m.x - center.x - gs.pan.x) / old_zoom;
    const float wy = (m.y - center.y - gs.pan.y) / old_zoom;
    gs.pan.x = m.x - center.x - wx * gs.zoom;
    gs.pan.y = m.y - center.y - wy * gs.zoom;
  }

  const float zoom = gs.zoom;
  auto to_screen = [&](ImVec2 w) {
    return ImVec2(center.x + gs.pan.x + w.x * zoom,
                  center.y + gs.pan.y + w.y * zoom);
  };

  // Layout: subscribers we publish to on the RIGHT, publishers we subscribe to
  // on the LEFT. Direction read from side -> no arrowheads needed.
  std::vector<Slot> slots;
  layout_column(view.publishes, +kColX, slots);
  layout_column(view.subscribes, -kColX, slots);

  const ImVec2 box_half(kBoxW * 0.5f * zoom, kBoxH * 0.5f * zoom);
  const float label_fs = std::clamp(ImGui::GetFontSize() * zoom, 9.0f, 26.0f);
  ImFont * font = ImGui::GetFont();

  auto draw_box = [&](ImVec2 c_screen, const std::string & label, ImU32 fill,
                      ImU32 border) {
    const ImVec2 a(c_screen.x - box_half.x, c_screen.y - box_half.y);
    const ImVec2 b(c_screen.x + box_half.x, c_screen.y + box_half.y);
    dl->AddRectFilled(a, b, fill, 5.0f);
    dl->AddRect(a, b, border, 5.0f, 0, 1.5f);
    const ImVec2 ts = font->CalcTextSizeA(label_fs, FLT_MAX, 0.0f, label.c_str());
    ImVec2 tp(c_screen.x - ts.x * 0.5f, c_screen.y - ts.y * 0.5f);
    dl->PushClipRect(a, b, true);
    dl->AddText(font, label_fs, tp, pal::text, label.c_str());
    dl->PopClipRect();
  };

  const ImVec2 center_screen = to_screen(ImVec2(0, 0));

  // Records for screen-space hit-testing after drawing.
  struct IconHit { ImVec2 c; float r; const Connection * conn; };
  struct NodeHit { ImVec2 c; ImVec2 half; std::string fq; };
  std::vector<IconHit> icon_hits;
  std::vector<NodeHit> node_hits;

  // Edges + peer boxes + edge icons.
  for (const auto & s : slots) {
    const ImVec2 peer_screen = to_screen(s.world);
    const EdgeStatus st = s.conn->status();
    const ImU32 scol = status_color(st);
    const ImU32 line_col = with_alpha(scol, 0.85f);

    dl->AddLine(center_screen, peer_screen, line_col, std::max(1.5f, 2.0f * zoom));

    // Mid-edge: topic label (hidden when zoomed out) + status icon beside it.
    const ImVec2 mid((center_screen.x + peer_screen.x) * 0.5f,
                     (center_screen.y + peer_screen.y) * 0.5f);
    const float ir = std::max(5.0f, kIconR * zoom);
    if (zoom >= kLabelHideZoom) {
      const ImVec2 ts =
        font->CalcTextSizeA(label_fs, FLT_MAX, 0.0f, s.conn->topic.c_str());
      const ImVec2 tp(mid.x - ts.x * 0.5f, mid.y - ts.y - ir - 2.0f);
      dl->AddRectFilled(ImVec2(tp.x - 3, tp.y - 1), ImVec2(tp.x + ts.x + 3, tp.y + ts.y + 1),
                        with_alpha(pal::panel, 0.85f), 3.0f);
      dl->AddText(font, label_fs, tp, pal::text_dim, s.conn->topic.c_str());
    }
    const ImVec2 icon_c(mid.x, mid.y + (zoom >= kLabelHideZoom ? 0.0f : 0.0f));
    dl->AddCircleFilled(icon_c, ir + 2.0f, pal::panel);
    draw_status_icon(dl, st, icon_c, ir);
    icon_hits.push_back({icon_c, ir + 3.0f, s.conn});

    draw_box(peer_screen, s.conn->peer_node, pal::panel, pal::edge);
    node_hits.push_back({peer_screen, box_half, s.conn->peer_node});
  }

  // Center (selected) node, drawn last so it sits on top of edge ends.
  draw_box(center_screen, view.name, pal::panel, pal::accent);

  dl->PopClipRect();

  // Screen-space hit-testing.
  std::string recenter;
  if (hovered) {
    const ImVec2 m = io.MousePos;
    
    // Check hover for tooltips
    for (const auto & h : icon_hits) {
      const float dx = m.x - h.c.x, dy = m.y - h.c.y;
      if (dx * dx + dy * dy <= h.r * h.r) {
        popup.request_hover(*h.conn);
        break;
      }
    }

    // Check clicks for centering
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
      const ImVec2 dd = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
      if (dd.x * dd.x + dd.y * dd.y < 36.0f) {  // < 6px => treat as click
        for (const auto & h : node_hits) {
          if (std::fabs(m.x - h.c.x) <= h.half.x &&
              std::fabs(m.y - h.c.y) <= h.half.y) {
            recenter = h.fq;
            break;
          }
        }
      }
    }
  }

  // Zoom hint / empty state.
  if (slots.empty()) {
    const char * msg = "(no connections)";
    const ImVec2 ts = ImGui::CalcTextSize(msg);
    dl->AddText(ImVec2(center.x - ts.x * 0.5f, center.y + kBoxH), pal::text_dim, msg);
  }

  return recenter;
}

}  // namespace rni
