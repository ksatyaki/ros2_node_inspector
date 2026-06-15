#include "view_graph.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "palette.hpp"

namespace ci {
namespace {

// World-space layout constants (pre-zoom).
constexpr float kIconR = 9.0f;    // status icon radius (world units)
constexpr float kLabelHideZoom = 0.55f;  // below this, draw icon only
constexpr float kConnSpacingY = 60.0f;   // Vertical space per connection (1.5x original)
constexpr float kPeerGapY = 40.0f;       // Vertical gap between peer nodes
constexpr float kBoxPadX = 30.0f;        // Horizontal padding inside box

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

  // Vertical scroll using mouse wheel to pan up/down instead of zoom
  if (hovered) {
    if (io.MouseWheel != 0.0f) {
      gs.pan.y += io.MouseWheel * 40.0f;
    }
    if (io.MouseWheelH != 0.0f) {
      gs.pan.x += io.MouseWheelH * 40.0f;
    }
  }
  
  // Enforce constant zoom as requested
  gs.zoom = 1.0f;

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

  const float label_fs = std::clamp(ImGui::GetFontSize() * zoom, 9.0f, 26.0f);
  ImFont * font = ImGui::GetFont();

  // Position peers in a transposed (left-to-right) layout
  const int n_peers = unique_peers.size();
  std::map<std::string, ImVec2> peer_positions;
  std::map<std::string, ImVec2> peer_sizes;

  float current_y = 0.0f;

  for (const auto& peer : unique_peers) {
    int num_conns = peer_connections[peer].size();
    float h = std::max(40.0f, num_conns * kConnSpacingY + 20.0f);
    float text_w = font->CalcTextSizeA(label_fs, FLT_MAX, 0.0f, peer.c_str()).x;
    float w = std::max(100.0f, text_w + kBoxPadX);
    peer_sizes[peer] = ImVec2(w, h);
    current_y += h + kPeerGapY;
  }

  float total_height = std::max(0.0f, current_y - kPeerGapY);
  float start_y = -total_height * 0.5f;
  float py = start_y;

  // X layout: Ego on left, Peers on right
  float ego_text_w = font->CalcTextSizeA(label_fs, FLT_MAX, 0.0f, view.name.c_str()).x;
  float ego_w = std::max(120.0f, ego_text_w + kBoxPadX);
  float ego_h = std::max(80.0f, total_height + 40.0f);

  // Position Ego and Peers based on window size
  float ego_right_edge = std::max(-350.0f, -canvas_sz.x * 0.35f);
  float peer_left_edge = std::min(350.0f, canvas_sz.x * 0.35f);
  
  float ego_x = ego_right_edge - ego_w * 0.5f;

  for (int i = 0; i < n_peers; ++i) {
    const auto& peer = unique_peers[i];
    ImVec2 sz = peer_sizes[peer];
    float cx = peer_left_edge + sz.x * 0.5f;
    float cy = py + sz.y * 0.5f;
    peer_positions[peer] = ImVec2(cx, cy);
    py += sz.y + kPeerGapY;
  }

  ImVec2 ego_pos(ego_x, 0.0f);

  auto draw_box = [&](ImVec2 c_screen, ImVec2 half_size, const std::string & label, ImU32 fill, ImU32 border) {
    const ImVec2 a(c_screen.x - half_size.x, c_screen.y - half_size.y);
    const ImVec2 b(c_screen.x + half_size.x, c_screen.y + half_size.y);
    dl->AddRectFilled(a, b, fill, 5.0f);
    dl->AddRect(a, b, border, 5.0f, 0, 1.5f);
    const ImVec2 ts = font->CalcTextSizeA(label_fs, FLT_MAX, 0.0f, label.c_str());
    ImVec2 tp(c_screen.x - ts.x * 0.5f, c_screen.y - ts.y * 0.5f);
    dl->PushClipRect(a, b, true);
    dl->AddText(font, label_fs, tp, pal::text, label.c_str());
    dl->PopClipRect();
  };

  const ImVec2 center_screen = to_screen(ego_pos);
  const ImVec2 ego_half(ego_w * 0.5f * zoom, ego_h * 0.5f * zoom);

  // Records for screen-space hit-testing after drawing.
  struct EdgeHit { ImVec2 min; ImVec2 max; const Connection * conn; };
  struct NodeHit { ImVec2 c; ImVec2 half; std::string fq; };
  std::vector<EdgeHit> edge_hits;
  std::vector<NodeHit> node_hits;

  // Pass 1: Draw lines and arrowheads.
  for (const auto& peer : unique_peers) {
    const ImVec2 peer_screen = to_screen(peer_positions[peer]);
    const auto& conns = peer_connections[peer];
    const int num_conns = static_cast<int>(conns.size());
    ImVec2 sz = peer_sizes[peer];
    const ImVec2 peer_half(sz.x * 0.5f * zoom, sz.y * 0.5f * zoom);

    for (int j = 0; j < num_conns; ++j) {
      const Connection* conn = conns[j];
      const float offset_y = (j - (num_conns - 1) * 0.5f) * kConnSpacingY * zoom;
      
      const ImVec2 p0(center_screen.x, peer_screen.y + offset_y);
      const ImVec2 p1(peer_screen.x, peer_screen.y + offset_y);

      const EdgeStatus st = conn->status();
      const ImU32 scol = status_color(st);
      const ImU32 line_col = with_alpha(scol, 0.85f);

      dl->AddLine(p0, p1, line_col, std::max(1.5f, 2.0f * zoom));

      // Draw arrowhead
      ImVec2 tip;
      ImVec2 arrow_dir;
      
      if (conn->direction == Direction::Publishes) {
        // Ego to Peer -> Pointing right (x positive)
        arrow_dir = ImVec2(1.0f, 0.0f);
        tip = ImVec2(p1.x - peer_half.x, p1.y);
      } else {
        // Peer to Ego -> Pointing left (x negative)
        arrow_dir = ImVec2(-1.0f, 0.0f);
        tip = ImVec2(p0.x + ego_half.x, p0.y);
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
    ImVec2 sz = peer_sizes[peer];
    const ImVec2 peer_half(sz.x * 0.5f * zoom, sz.y * 0.5f * zoom);

    const float ir = std::max(5.0f, kIconR * zoom);

    for (int j = 0; j < num_conns; ++j) {
      const Connection* conn = conns[j];
      const float offset_y = (j - (num_conns - 1) * 0.5f) * kConnSpacingY * zoom;
      
      const ImVec2 p0(center_screen.x, peer_screen.y + offset_y);
      const ImVec2 p1(peer_screen.x, peer_screen.y + offset_y);

      const EdgeStatus st = conn->status();
      const ImVec2 mid((p0.x + p1.x) * 0.5f, p1.y);

      const bool draw_label = (zoom >= kLabelHideZoom);
      ImVec2 ts(0, 0);
      if (draw_label) {
        ts = font->CalcTextSizeA(label_fs, FLT_MAX, 0.0f, conn->topic.c_str());
      }

      const float gap = 6.0f;
      // total width of icon + text block
      const float total_w = (draw_label) ? (ir * 2.0f + gap + ts.x) : (ir * 2.0f);
      
      // We want the ENTIRE block (icon + text) to be centered exactly at `mid.x`.
      const float block_left = mid.x - total_w * 0.5f;
      
      // Icon is the left-most element
      const ImVec2 icon_c(block_left + ir, mid.y);
      
      if (draw_label) {
        const ImVec2 tp(block_left + ir * 2.0f + gap, mid.y - ts.y * 0.5f);
        dl->AddRectFilled(ImVec2(tp.x - 3, tp.y - 1), ImVec2(tp.x + ts.x + 3, tp.y + ts.y + 1),
                          pal::panel, 3.0f);
        const ImU32 text_col = status_color(st);
        dl->AddText(font, label_fs, tp, text_col, conn->topic.c_str());
      }
      dl->AddCircleFilled(icon_c, ir + 2.0f, pal::panel);
      draw_status_icon(dl, st, icon_c, ir);
      
      ImVec2 hit_min(block_left - 3.0f, mid.y - std::max(ir + 3.0f, ts.y * 0.5f + 1.0f));
      ImVec2 hit_max(mid.x + total_w * 0.5f + 3.0f, mid.y + std::max(ir + 3.0f, ts.y * 0.5f + 1.0f));
      edge_hits.push_back({hit_min, hit_max, conn});
    }

    draw_box(peer_screen, peer_half, peer, pal::panel, pal::edge);
    node_hits.push_back({peer_screen, peer_half, peer});
  }

  // Center (selected) node, drawn last so it sits on top of edge ends.
  draw_box(center_screen, ego_half, view.name, pal::panel, pal::accent);

  dl->PopClipRect();

  // Screen-space hit-testing.
  std::string recenter;
  if (hovered) {
    const ImVec2 m = io.MousePos;
    
    // Check hover for tooltips
    for (const auto & h : edge_hits) {
      if (m.x >= h.min.x && m.x <= h.max.x && m.y >= h.min.y && m.y <= h.max.y) {
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
    dl->AddText(ImVec2(center.x - ts.x * 0.5f, center.y + 40.0f), pal::text_dim, msg);
  }

  return recenter;
}

}  // namespace ci
