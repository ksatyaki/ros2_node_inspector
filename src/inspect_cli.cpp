// Manual harness for M1: exercises graph_model + liveness_probe against a live
// graph (e.g. ros2 run demo_nodes_cpp talker / listener). Not a GUI; the GUI
// lands in M2. Usage:
//   inspect_cli                 # list all nodes
//   inspect_cli <node> [<ns>]   # inspect one node for ~6 s and print its edges

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

#include "rclcpp/rclcpp.hpp"

#include "graph_model.hpp"
#include "liveness_probe.hpp"
#include "status.hpp"

namespace {

const char * status_word(ci::EdgeStatus s)
{
  switch (s) {
    case ci::EdgeStatus::Ok:           return "OK   ";
    case ci::EdgeStatus::Latched:      return "LATCH";
    case ci::EdgeStatus::QosMismatch:  return "QOS? ";
    case ci::EdgeStatus::TypeMismatch: return "TYPEX";
    case ci::EdgeStatus::Dead:         return "DEAD ";
    case ci::EdgeStatus::Unknown:      return "?    ";
  }
  return "?    ";
}

void print_edges(const char * heading, const std::vector<ci::Connection> & edges)
{
  std::printf("  %s\n", heading);
  if (edges.empty()) {
    std::printf("    (none)\n");
  }
  for (const auto & c : edges) {
    std::printf("    [%s] %s  ->  %s   %s\n",
                status_word(c.status()), c.topic.c_str(),
                c.peer_node.c_str(), ci::status_detail(c).c_str());
  }
}

}  // namespace

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<rclcpp::Node>("connection_inspector_cli");

  // Let discovery settle.
  rclcpp::executors::SingleThreadedExecutor exec;
  exec.add_node(node);
  std::thread spin([&exec]() { exec.spin(); });
  std::this_thread::sleep_for(std::chrono::seconds(1));

  if (argc < 2) {
    std::printf("Live nodes:\n");
    for (const auto & [name, ns] :
         node->get_node_graph_interface()->get_node_names_and_namespaces())
    {
      std::printf("  %s%s\n", ns == "/" ? "/" : (ns + "/").c_str(), name.c_str());
    }
    std::printf("\nUsage: inspect_cli <node_name> [namespace]\n");
    exec.cancel();
    spin.join();
    rclcpp::shutdown();
    return 0;
  }

  const std::string name = argv[1];
  const std::string ns = argc >= 3 ? argv[2] : "/";

  auto view = ci::build_node_view(*node, name, ns);
  ci::LivenessProbe probe;
  probe.reconfigure(*node, view);

  std::printf("Inspecting %s — measuring liveness for ~6 s…\n", view.name.c_str());
  std::this_thread::sleep_for(std::chrono::seconds(6));

  probe.apply(view);
  const auto counts = ci::count_connections(view);
  std::printf("Node %s   (type/QoS counts: ok=%d warn=%d err=%d)\n",
              view.name.c_str(), counts.ok, counts.warn, counts.err);
  print_edges("Publishes ->", view.publishes);
  print_edges("<- Subscribes", view.subscribes);

  exec.cancel();
  spin.join();
  rclcpp::shutdown();
  return 0;
}
