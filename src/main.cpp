// ROS 2 Node Connection Inspector — GUI entry point.
//
// Threading (see plan.md "Threading"): a single background "ROS thread" owns the
// rclcpp node, executor, graph reads and the liveness probe — it services
// discovery + probe callbacks (spin_some) and, every ~500 ms, rebuilds the node
// list + per-node counts + the selected node's NodeView and publishes a small
// snapshot under one mutex. The main thread runs the GLFW/ImGui frame loop and
// only ever reads that snapshot (copied under lock, no lock held during render).
// Keeping every rclcpp call on the one ROS thread sidesteps subscription
// lifetime races entirely.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <GLFW/glfw3.h>

#include "rclcpp/rclcpp.hpp"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include "graph_model.hpp"
#include "icons.hpp"
#include "liveness_probe.hpp"
#include "palette.hpp"
#include "view_graph.hpp"
#include "view_table.hpp"
#include "views_common.hpp"

namespace {

// ---- Shared snapshot between the ROS thread (writer) and UI thread (reader) --

struct NodeEntry {
  std::string fq;    // fully-qualified "/ns/name"
  std::string name;  // bare node name
  std::string ns;    // namespace ("/" or "/ns")
  rni::StatusCounts counts;
};

struct Shared {
  std::mutex mtx;
  std::vector<NodeEntry> nodes;     // all live nodes + their type/QoS counts
  rni::NodeView selected_view;      // ego view of the currently selected node
  std::string selected_fq;          // which node selected_view describes
  std::string requested_fq;         // UI -> ROS: desired selection
  bool has_request = false;
};

std::atomic<bool> g_running{true};

// Split a fully-qualified node name into (namespace, bare name).
// "/talker" -> ("/", "talker") ; "/ns/talker" -> ("/ns", "talker").
void split_fq(const std::string & fq, std::string & ns, std::string & name)
{
  const auto slash = fq.find_last_of('/');
  if (slash == std::string::npos) {
    ns = "/";
    name = fq;
    return;
  }
  name = fq.substr(slash + 1);
  ns = (slash == 0) ? "/" : fq.substr(0, slash);
}

// ---- ROS thread: discovery, counts, selected view, liveness --------------

void ros_thread(rclcpp::Node::SharedPtr node, Shared * sh)
{
  rclcpp::executors::SingleThreadedExecutor exec;
  exec.add_node(node);
  rni::LivenessProbe probe;
  std::string active_fq;  // node the probe is currently configured for

  auto last_refresh = std::chrono::steady_clock::now() -
                      std::chrono::milliseconds(1000);

  while (g_running.load()) {
    exec.spin_some(std::chrono::milliseconds(20));

    const auto now = std::chrono::steady_clock::now();
    if (now - last_refresh < std::chrono::milliseconds(500)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }
    last_refresh = now;

    // Pull any pending selection request from the UI.
    std::string want;
    {
      std::lock_guard<std::mutex> lk(sh->mtx);
      want = sh->has_request ? sh->requested_fq : sh->selected_fq;
    }

    // All live nodes + per-node type/QoS counts (no probing of unselected nodes).
    auto names = node->get_node_graph_interface()->get_node_names_and_namespaces();
    std::vector<NodeEntry> entries;
    entries.reserve(names.size());
    for (const auto & [name, ns] : names) {
      NodeEntry e;
      e.name = name;
      e.ns = ns;
      e.fq = (ns == "/" ? "/" : ns + "/") + name;
      
      // Filter out the inspector node itself from the list
      if (e.fq == std::string(node->get_fully_qualified_name())) {
        continue;
      }

      e.counts = rni::count_connections(rni::build_node_view(*node, name, ns));
      entries.push_back(std::move(e));
    }

    // Default selection to the first node if nothing valid is requested.
    bool valid = false;
    for (const auto & e : entries) {
      if (e.fq == want) { valid = true; break; }
    }
    if (!valid) {
      want = entries.empty() ? std::string{} : entries.front().fq;
    }

    // (Re)build the selected node's ego view + liveness probe.
    rni::NodeView view;
    if (!want.empty()) {
      std::string ns, name;
      split_fq(want, ns, name);
      view = rni::build_node_view(*node, name, ns);
      if (want != active_fq) {
        probe.clear();          // selection changed: drop old probes
        active_fq = want;
      }
      
      // Do not probe ourselves, to prevent self-inspection feedback loops.
      if (want == std::string(node->get_fully_qualified_name())) {
        probe.clear();
      } else {
        probe.reconfigure(*node, view);
        probe.apply(view);
      }
    } else {
      probe.clear();
      active_fq.clear();
    }

    {
      std::lock_guard<std::mutex> lk(sh->mtx);
      sh->nodes = std::move(entries);
      sh->selected_view = std::move(view);
      sh->selected_fq = want;
      sh->has_request = false;
    }
  }
}

// ---- Dropdown with per-node ✓N ?N ✗N counts ------------------------------

// Inline colored counts. Uses FontAwesome glyphs if available, else letters.
void draw_counts_inline(const rni::StatusCounts & c)
{
  const bool fa = rni::fontawesome_loaded();
  ImGui::SameLine();
  ImGui::TextColored(pal::to_vec4(pal::ok), fa ? "\xef\x81\x98 %d" : "ok %d", c.ok);
  ImGui::SameLine();
  ImGui::TextColored(pal::to_vec4(pal::warn), fa ? "\xef\x81\x99 %d" : "? %d", c.warn);
  ImGui::SameLine();
  ImGui::TextColored(pal::to_vec4(pal::error), fa ? "\xef\x81\x97 %d" : "x %d", c.err);
}

// Returns the fq of a newly selected node, or "" if selection unchanged.
std::string draw_left_pane(const std::vector<NodeEntry> & nodes,
                           const std::string & selected_fq)
{
  std::string result;
  std::vector<NodeEntry> sorted = nodes;
  std::sort(sorted.begin(), sorted.end(), [](const auto &a, const auto &b){ return a.fq < b.fq; });

  for (const auto & e : sorted) {
    const bool sel = (e.fq == selected_fq);
    if (ImGui::Selectable(e.fq.c_str(), sel, ImGuiSelectableFlags_AllowOverlap)) {
      result = e.fq;
    }
    draw_counts_inline(e.counts);
  }
  return result;
}

void glfw_error(int err, const char * desc)
{
  std::fprintf(stderr, "GLFW error %d: %s\n", err, desc);
}

}  // namespace

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<rclcpp::Node>("ros2_node_inspector_gui");

  Shared shared;
  std::thread ros(ros_thread, node, &shared);

  // ---- Window + ImGui bootstrap ----
  glfwSetErrorCallback(glfw_error);
  if (!glfwInit()) {
    std::fprintf(stderr, "Failed to init GLFW\n");
    g_running = false;
    ros.join();
    rclcpp::shutdown();
    return 1;
  }
  const char * glsl_version = "#version 130";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  GLFWwindow * window =
    glfwCreateWindow(1100, 720, "ROS 2 Node Connection Inspector", nullptr, nullptr);
  if (!window) {
    std::fprintf(stderr, "Failed to create GLFW window\n");
    glfwTerminate();
    g_running = false;
    ros.join();
    rclcpp::shutdown();
    return 1;
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);  // vsync

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::GetIO().IniFilename = nullptr;  // don't litter an imgui.ini
  pal::apply_palette();

  rni::load_ui_font(16.0f);          // DejaVuSans (Latin+arrows) or built-in
  rni::try_load_fontawesome(14.0f);  // optional toolbar/legend glyphs (merged)

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  // UI-side persistent state.
  rni::StatusPopup popup;
  rni::GraphState graph_state;
  std::string last_selected;
  int tab = 1;  // 0 = table, 1 = graph
  float font_size_px = 16.0f;
  bool needs_font_rebuild = false;

  while (!glfwWindowShouldClose(window) && rclcpp::ok()) {
    glfwPollEvents();

    if (needs_font_rebuild) {
      ImGui_ImplOpenGL3_DestroyFontsTexture();
      ImGui::GetIO().Fonts->Clear();
      rni::load_ui_font(font_size_px);
      rni::try_load_fontawesome(font_size_px * (14.0f / 16.0f));
      ImGui_ImplOpenGL3_CreateFontsTexture();
      needs_font_rebuild = false;
    }

    // Copy the snapshot under lock; render from the copy (no lock during ImGui).
    std::vector<NodeEntry> nodes;
    rni::NodeView view;
    std::string selected_fq;
    {
      std::lock_guard<std::mutex> lk(shared.mtx);
      nodes = shared.nodes;
      view = shared.selected_view;
      selected_fq = shared.selected_fq;
    }
    if (selected_fq != last_selected) {
      graph_state.centered = false;  // recenter pan on selection change
      last_selected = selected_fq;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    const ImGuiViewport * vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::Begin("Inspector", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImGui::BeginChild("left_pane", ImVec2(320.0f, 0), true);
    ImGui::TextDisabled("Nodes (%zu)", nodes.size());
    ImGui::SameLine();
    if (ImGui::Button("-")) {
      font_size_px = std::max(10.0f, font_size_px - 1.0f);
      needs_font_rebuild = true;
    }
    ImGui::SameLine();
    ImGui::Text("%.0f px", font_size_px);
    ImGui::SameLine();
    if (ImGui::Button("+")) {
      font_size_px = std::min(48.0f, font_size_px + 1.0f);
      needs_font_rebuild = true;
    }
    ImGui::Separator();
    const std::string picked = draw_left_pane(nodes, selected_fq);
    if (!picked.empty()) {
      std::lock_guard<std::mutex> lk(shared.mtx);
      shared.requested_fq = picked;
      shared.has_request = true;
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("right_pane", ImVec2(0, 0), false);

    if (ImGui::BeginTabBar("views")) {
      if (ImGui::BeginTabItem("Graph")) {
        tab = 1;
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("Table")) {
        tab = 0;
        ImGui::EndTabItem();
      }
      ImGui::EndTabBar();
    }

    ImGui::BeginChild("body", ImVec2(0, 0), false);
    if (selected_fq.empty()) {
      ImGui::TextDisabled("No nodes discovered yet…");
    } else if (tab == 0) {
      rni::render_table_view(view, popup);
    } else {
      const std::string recenter = rni::render_graph_view(view, graph_state, popup);
      if (!recenter.empty()) {
        std::lock_guard<std::mutex> lk(shared.mtx);
        shared.requested_fq = recenter;
        shared.has_request = true;
      }
    }
    ImGui::EndChild();
    ImGui::EndChild();

    popup.render();
    ImGui::End();

    ImGui::Render();
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    glViewport(0, 0, w, h);
    const ImVec4 bg = pal::to_vec4(pal::bg);
    glClearColor(bg.x, bg.y, bg.z, bg.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);
  }

  g_running = false;
  ros.join();

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  glfwDestroyWindow(window);
  glfwTerminate();
  rclcpp::shutdown();
  return 0;
}
