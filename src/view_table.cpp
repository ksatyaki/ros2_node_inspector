#include "view_table.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "palette.hpp"

namespace rni {
namespace {

// Rate cell text: "30 Hz" when live, "DEAD" when proven silent, "—" while the
// probe window has not yet elapsed (Unknown).
std::string rate_text(const Connection & c)
{
  if (!c.hz_known) {
    return "\xe2\x80\x94";  // em dash
  }
  if (c.hz == 0.0) {
    return "DEAD";
  }
  char buf[32];
  std::snprintf(buf, sizeof(buf), c.hz < 10.0 ? "%.1f Hz" : "%.0f Hz", c.hz);
  return buf;
}

void draw_section(const char * heading, const std::vector<Connection> & edges,
                  const char * table_id, StatusPopup & popup)
{
  ImGui::SeparatorText(heading);
  if (edges.empty()) {
    ImGui::TextDisabled("  (none)");
    return;
  }

  const ImGuiTableFlags flags = ImGuiTableFlags_Borders |
                                ImGuiTableFlags_RowBg |
                                ImGuiTableFlags_Resizable |
                                ImGuiTableFlags_SizingStretchProp;
  if (!ImGui::BeginTable(table_id, 5, flags)) {
    return;
  }
  ImGui::TableSetupColumn("Topic", ImGuiTableColumnFlags_WidthStretch, 0.28f);
  ImGui::TableSetupColumn("Peer node", ImGuiTableColumnFlags_WidthStretch, 0.24f);
  ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 0.30f);
  ImGui::TableSetupColumn("Rate", ImGuiTableColumnFlags_WidthFixed, 70.0f);
  ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 50.0f);
  ImGui::TableHeadersRow();

  int row = 0;
  for (const auto & c : edges) {
    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(c.topic.c_str());

    ImGui::TableSetColumnIndex(1);
    ImGui::TextUnformatted(c.peer_node.c_str());

    ImGui::TableSetColumnIndex(2);
    ImGui::PushStyleColor(ImGuiCol_Text, pal::to_vec4(pal::text_dim));
    ImGui::TextUnformatted(c.my_type.c_str());
    ImGui::PopStyleColor();

    ImGui::TableSetColumnIndex(3);
    ImGui::TextUnformatted(rate_text(c).c_str());

    ImGui::TableSetColumnIndex(4);
    const float h = ImGui::GetTextLineHeight();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    char id[64];
    std::snprintf(id, sizeof(id), "##st_%s_%d", table_id, row);
    icon_button(id, p, h);
    if (ImGui::IsItemHovered()) {
      popup.request_hover(c);
    }
    draw_status_icon(ImGui::GetWindowDrawList(), c.status(),
                     ImVec2(p.x + h * 0.5f, p.y + h * 0.5f), h * 0.42f);
    ++row;
  }
  ImGui::EndTable();
}

}  // namespace

void render_table_view(const NodeView & view, StatusPopup & popup)
{
  bool determining = false;
  for (const auto & c : view.publishes) if (!c.hz_known) determining = true;
  for (const auto & c : view.subscribes) if (!c.hz_known) determining = true;

  if (determining) {
    float alpha = 0.5f + 0.5f * std::sin(ImGui::GetTime() * 5.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, alpha));
    const char * msg = "Determining rate...";
    float w = ImGui::CalcTextSize(msg).x;
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - w) * 0.5f);
    ImGui::TextUnformatted(msg);
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 6));
  }

  draw_section("Publishes \xe2\x86\x92", view.publishes, "tbl_pub", popup);
  ImGui::Dummy(ImVec2(0, 6));
  draw_section("\xe2\x86\x90 Subscribes", view.subscribes, "tbl_sub", popup);
}

}  // namespace rni
