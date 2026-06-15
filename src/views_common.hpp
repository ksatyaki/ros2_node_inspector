#pragma once

#include <string>

#include "imgui.h"

#include "graph_model.hpp"
#include "icons.hpp"
#include "status.hpp"

namespace ci {

// Shared status-detail popup, opened by clicking a status icon in either view.
// Click-driven (OpenPopup), dismissed on click-away. Same call site for table
// and graph: a view calls request() with the clicked connection, then render()
// is called once per frame after both views.
class StatusPopup {
public:
  // Stash the plain-language sentence and flag the popup to open this frame.
  void request(const Connection & c)
  {
    text_ = status_detail(c);
    color_ = status_color(c.status());
    open_ = true;
  }

  void request_hover(const Connection & c)
  {
    text_ = status_detail(c);
    color_ = status_color(c.status());
    hovered_this_frame_ = true;
  }

  void render()
  {
    // The old click-driven popup logic
    if (open_) {
      ImGui::OpenPopup("status_detail");
      open_ = false;
    }
    if (ImGui::BeginPopup("status_detail")) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImColor(color_).Value);
      ImGui::TextUnformatted("\xe2\x97\x8f");  // bullet in the status color
      ImGui::PopStyleColor();
      ImGui::SameLine();
      ImGui::TextUnformatted(text_.c_str());
      ImGui::EndPopup();
    }

    // The new hover-driven tooltip logic
    if (hovered_this_frame_) {
      ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(0x1A, 0x1B, 0x26, 0xFF)); // old default bg
      ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0x7A, 0xA2, 0xF7, 0xFF)); // old default accent
      ImGui::BeginTooltip();
      ImGui::PushStyleColor(ImGuiCol_Text, ImColor(color_).Value);
      ImGui::TextUnformatted("\xe2\x97\x8f");
      ImGui::PopStyleColor();
      ImGui::SameLine();
      ImGui::TextUnformatted(text_.c_str());
      ImGui::EndTooltip();
      ImGui::PopStyleColor(2);
      hovered_this_frame_ = false;
    }
  }

private:
  std::string text_;
  ImU32 color_ = 0;
  bool open_ = false;
  bool hovered_this_frame_ = false;
};

// An invisible button matching the icon's bounding box, so a click anywhere on
// the marker registers. `id` must be unique per connection within a view.
inline bool icon_button(const char * id, ImVec2 top_left, float box)
{
  ImGui::SetCursorScreenPos(top_left);
  return ImGui::InvisibleButton(id, ImVec2(box, box));
}

}  // namespace ci
