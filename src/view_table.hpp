#pragma once

#include "graph_model.hpp"
#include "views_common.hpp"

namespace ci {

// Render the selected node's connections as two tables — "Publishes ->" and
// "<- Subscribes" — each row carrying a clickable status icon. Clicking an icon
// requests the shared status popup.
void render_table_view(const NodeView & view, StatusPopup & popup);

}  // namespace ci
