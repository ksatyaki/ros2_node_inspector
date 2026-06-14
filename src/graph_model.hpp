#pragma once

#include <string>
#include <vector>

#include "rclcpp/rclcpp.hpp"

#include "qos_compat.hpp"
#include "status.hpp"

namespace rni {

// Direction of a connection relative to the owning (selected) node.
//   Publishes  : the owning node publishes the topic; the peer subscribes.
//   Subscribes : the owning node subscribes the topic; the peer publishes.
enum class Direction { Publishes, Subscribes };

struct Connection {
  std::string topic;
  std::string self_node;   // fully-qualified name of the owning node
  std::string peer_node;   // fully-qualified name of the node on the other end
  std::string my_type;     // type string declared by the owning node
  std::string peer_type;   // type string declared by the peer
  rclcpp::QoS my_qos{10};
  rclcpp::QoS peer_qos{10};
  Direction   direction = Direction::Publishes;
  bool        type_match = true;     // my_type == peer_type
  QosVerdict  qos;                   // RxO verdict (offered=publisher, requested=subscriber)
  double      hz = 0.0;              // measured rate, 0.0 if dead/unknown
  bool        hz_known = false;      // false until first probe window elapses

  EdgeStatus status() const { return compute_status(*this); }

  // Publisher/subscriber roles resolved from direction (for popup sentences).
  const std::string & pub_node()  const { return direction == Direction::Publishes ? self_node : peer_node; }
  const std::string & sub_node()  const { return direction == Direction::Publishes ? peer_node : self_node; }
  const std::string & pub_type()  const { return direction == Direction::Publishes ? my_type  : peer_type; }
  const std::string & sub_type()  const { return direction == Direction::Publishes ? peer_type : my_type; }
};

struct NodeView {
  std::string name;                   // fully-qualified node name
  std::vector<Connection> publishes;  // topics this node publishes -> its subscribers
  std::vector<Connection> subscribes; // topics this node subscribes -> its publishers
};

// Aggregate counts for the dropdown, computed from graph metadata only
// (type + QoS). Liveness is deliberately excluded — unselected nodes are never
// probed. Mapping: ok = type-OK & QoS-OK, warn = QoS-incompatible,
// err = type-mismatch. Type mismatch takes precedence over QoS.
struct StatusCounts {
  int ok = 0;
  int warn = 0;
  int err = 0;
};

StatusCounts count_connections(const NodeView & view);

// Build the ego NodeView for node (name, ns) from the live rclcpp graph.
// Resolves peers, types and QoS for every topic the node publishes/subscribes.
// Liveness (hz) is left unset here; it is filled later by the liveness probe.
NodeView build_node_view(rclcpp::Node & node,
                         const std::string & name,
                         const std::string & ns);

}  // namespace rni
