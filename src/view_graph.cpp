#include "view_graph.hpp"

#include <algorithm>
#include <cmath>
#include <map>
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

// (Layout removed)

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

  // Group connections by peer
  std::vector<std::string> unique_peers;
  std::map<std::string, std::vector<const Connection*>> peer_connections;

  auto add_connections = [&](const std::vector<Connection>& conns) {
    for (const auto& c : conns) {
      if (std::find(unique_peers.begin(), unique_peers.end(), c.peer_node) == unique_peers.end()) {
        unique_peers.push_back(c.peer_node);
      }
      peer_connections[c.peer_node].push_back(&c);
    }
  };
  add_connections(view.publishes);
  add_connections(view.subscribes);

  // Position peers in a circle
  const int n_peers = unique_peers.size();
  std::map<std::string, ImVec2> peer_positions;
  const float PI = 3.14159265f;
  for (int i = 0; i < n_peers; ++i) {
    float angle = i * 2.0f * PI / std::max(1, n_peers) - PI / 2.0f;
    peer_positions[unique_peers[i]] = ImVec2(kColX * std::cos(angle), kColX * std::sin(angle));
  }

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

  // Pass 1: Draw lines and arrowheads.
  for (const auto& peer : unique_peers) {
    const ImVec2 peer_screen = to_screen(peer_positions[peer]);
    const auto& conns = peer_connections[peer];
    const int num_conns = static_cast<int>(conns.size());

    ImVec2 dir(peer_screen.x - center_screen.x, peer_screen.y - center_screen.y);
    const float len = std::hypot(dir.x, dir.y);
    ImVec2 norm(0, 0);
    ImVec2 d_norm(0, 0);
    if (len > 0) {
      d_norm = ImVec2(dir.x / len, dir.y / len);
      norm = ImVec2(-d_norm.y, d_norm.x);
    }

    const float edge_spacing = 30.0f * zoom;

    for (int j = 0; j < num_conns; ++j) {
      const Connection* conn = conns[j];
      const float offset = (j - (num_conns - 1) * 0.5f) * edge_spacing;
      
      const ImVec2 p0(center_screen.x + norm.x * offset, center_screen.y + norm.y * offset);
      const ImVec2 p1(peer_screen.x + norm.x * offset, peer_screen.y + norm.y * offset);

      const EdgeStatus st = conn->status();
      const ImU32 scol = status_color(st);
      const ImU32 line_col = with_alpha(scol, 0.85f);

      dl->AddLine(p0, p1, line_col, std::max(1.5f, 2.0f * zoom));

      // Draw arrowhead
      ImVec2 tip;
      ImVec2 arrow_dir;
      
      auto box_intersect = [](ImVec2 start, ImVec2 d, ImVec2 half) -> float {
        float t_min = FLT_MAX;
        if (std::abs(d.x) > 1e-5f) {
          float t1 = (half.x - start.x) / d.x;
          float t2 = (-half.x - start.x) / d.x;
          if (t1 > 0 && t1 < t_min) t_min = t1;
          if (t2 > 0 && t2 < t_min) t_min = t2;
        }
        if (std::abs(d.y) > 1e-5f) {
          float t1 = (half.y - start.y) / d.y;
          float t2 = (-half.y - start.y) / d.y;
          if (t1 > 0 && t1 < t_min) t_min = t1;
          if (t2 > 0 && t2 < t_min) t_min = t2;
        }
        return t_min == FLT_MAX ? 0.0f : t_min;
      };

      if (conn->direction == Direction::Publishes) {
        arrow_dir = d_norm;
        float t = box_intersect(ImVec2(norm.x * offset, norm.y * offset), 
                                ImVec2(-d_norm.x, -d_norm.y), box_half);
        tip = ImVec2(p1.x - d_norm.x * t, p1.y - d_norm.y * t);
      } else {
        arrow_dir = ImVec2(-d_norm.x, -d_norm.y);
        float t = box_intersect(ImVec2(norm.x * offset, norm.y * offset), 
                                d_norm, box_half);
        tip = ImVec2(p0.x + d_norm.x * t, p0.y + d_norm.y * t);
      }
      
      ImVec2 tip_p1(tip.x - arrow_dir.x * 12.0f * zoom + arrow_dir.y * 6.0f * zoom,
                    tip.y - arrow_dir.y * 12.0f * zoom - arrow_dir.x * 6.0f * zoom);
      ImVec2 tip_p2(tip.x - arrow_dir.x * 12.0f * zoom - arrow_dir.y * 6.0f * zoom,
                    tip.y - arrow_dir.y * 12.0f * zoom + arrow_dir.x * 6.0f * zoom);
      dl->AddTriangleFilled(tip, tip_p1, tip_p2, line_col);
    }
  }

  // Pass 2: Draw icons, text labels, and peer boxes.
  for (const auto& peer : unique_peers) {
    const ImVec2 peer_screen = to_screen(peer_positions[peer]);
    const auto& conns = peer_connections[peer];
    const int num_conns = static_cast<int>(conns.size());

    ImVec2 dir(peer_screen.x - center_screen.x, peer_screen.y - center_screen.y);
    const float len = std::hypot(dir.x, dir.y);
    ImVec2 norm(0, 0);
    ImVec2 d_norm(0, 0);
    if (len > 0) {
      d_norm = ImVec2(dir.x / len, dir.y / len);
      norm = ImVec2(-d_norm.y, d_norm.x);
    }

    const float edge_spacing = 30.0f * zoom;
    const float ir = std::max(5.0f, kIconR * zoom);

    for (int j = 0; j < num_conns; ++j) {
      const Connection* conn = conns[j];
      const float offset = (j - (num_conns - 1) * 0.5f) * edge_spacing;
      
      const ImVec2 p0(center_screen.x + norm.x * offset, center_screen.y + norm.y * offset);
      const ImVec2 p1(peer_screen.x + norm.x * offset, peer_screen.y + norm.y * offset);

      const EdgeStatus st = conn->status();
      const ImVec2 mid((p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f);

      ImVec2 up_norm(-d_norm.y, d_norm.x);
      if (up_norm.y > 0) {
        up_norm.x = -up_norm.x;
        up_norm.y = -up_norm.y;
      }

      if (zoom >= kLabelHideZoom) {
        const ImVec2 ts = font->CalcTextSizeA(label_fs, FLT_MAX, 0.0f, conn->topic.c_str());
        ImVec2 tp_center(mid.x + up_norm.x * (ir + ts.y * 0.5f + 4.0f), 
                         mid.y + up_norm.y * (ir + ts.y * 0.5f + 4.0f));
        const ImVec2 tp(tp_center.x - ts.x * 0.5f, tp_center.y - ts.y * 0.5f);
        dl->AddRectFilled(ImVec2(tp.x - 3, tp.y - 1), ImVec2(tp.x + ts.x + 3, tp.y + ts.y + 1),
                          pal::panel, 3.0f);
        dl->AddText(font, label_fs, tp, pal::text_dim, conn->topic.c_str());
      }
      const ImVec2 icon_c(mid.x, mid.y);
      dl->AddCircleFilled(icon_c, ir + 2.0f, pal::panel);
      draw_status_icon(dl, st, icon_c, ir);
      icon_hits.push_back({icon_c, ir + 3.0f, conn});
    }

    draw_box(peer_screen, peer, pal::panel, pal::edge);
    node_hits.push_back({peer_screen, box_half, peer});
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
  if (unique_peers.empty()) {
    const char * msg = "(no connections)";
    const ImVec2 ts = ImGui::CalcTextSize(msg);
    dl->AddText(ImVec2(center.x - ts.x * 0.5f, center.y + kBoxH), pal::text_dim, msg);
  }

  return recenter;
}

}  // namespace rni
