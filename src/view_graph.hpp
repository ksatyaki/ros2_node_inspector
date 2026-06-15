#pragma once

#include <string>

#include "imgui.h"

#include "graph_model.hpp"
#include "views_common.hpp"

namespace ci {

// Persistent pan/zoom for the graph canvas (survives across frames/refreshes).
struct GraphState {
  ImVec2 pan{0.0f, 0.0f};  // screen-pixel offset from canvas center
  float zoom = 1.0f;
  bool centered = false;   // reset pan once when a new node is selected
};

// Hand-drawn ego graph: selected node centered, publishers it subscribes to on
// the left, subscribers it publishes to on the right. Each edge carries the
// topic label + a clickable status icon. Pan by dragging empty canvas, zoom with
// the wheel (about the cursor). Clicking a status icon opens the shared popup;
// clicking a peer node box returns that peer's fully-qualified name so the caller
// can recenter on it. Returns "" if no recenter was requested this frame.
std::string render_graph_view(const NodeView & view, GraphState & gs,
                              StatusPopup & popup);

}  // namespace ci
