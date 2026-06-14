#include "graph_model.hpp"

#include <map>
#include <string>
#include <vector>

#include "rclcpp/rclcpp.hpp"

namespace rni {

StatusCounts count_connections(const NodeView & view)
{
  StatusCounts counts;
  auto classify = [&counts](const Connection & c) {
    switch (c.status()) {
      case EdgeStatus::Ok:
      case EdgeStatus::Latched:
      case EdgeStatus::Unknown:
        ++counts.ok;
        break;
      case EdgeStatus::QosMismatch:
        ++counts.warn;
        break;
      case EdgeStatus::TypeMismatch:
      case EdgeStatus::Dead:
        ++counts.err;
        break;
    }
  };
  for (const auto & c : view.publishes) classify(c);
  for (const auto & c : view.subscribes) classify(c);
  return counts;
}

namespace {

std::string fq_name(const std::string & ns, const std::string & name)
{
  if (ns.empty() || ns == "/") {
    return "/" + name;
  }
  return ns + "/" + name;
}

// Find the QoS that node (name, ns) declares for topic in an endpoint list.
rclcpp::QoS find_self_qos(const std::vector<rclcpp::TopicEndpointInfo> & infos,
                          const std::string & name,
                          const std::string & ns,
                          const rclcpp::QoS & fallback)
{
  for (const auto & info : infos) {
    if (info.node_name() == name && info.node_namespace() == ns) {
      return info.qos_profile();
    }
  }
  return fallback;
}

}  // namespace

NodeView build_node_view(rclcpp::Node & node,
                         const std::string & name,
                         const std::string & ns)
{
  NodeView view;
  view.name = fq_name(ns, name);

  auto graph = node.get_node_graph_interface();

  // --- Topics this node PUBLISHES; peers are subscribers. ---
  const auto pub_topics = graph->get_publisher_names_and_types_by_node(name, ns);
  for (const auto & [topic, types] : pub_topics) {
    const std::string my_type = types.empty() ? std::string{} : types.front();

    const auto pub_infos = node.get_publishers_info_by_topic(topic);
    const rclcpp::QoS my_qos = find_self_qos(pub_infos, name, ns, rclcpp::QoS{10});

    for (const auto & sub : node.get_subscriptions_info_by_topic(topic)) {
      Connection c;
      c.topic = topic;
      c.self_node = view.name;
      c.peer_node = fq_name(sub.node_namespace(), sub.node_name());
      c.my_type = my_type;
      c.peer_type = sub.topic_type();
      c.my_qos = my_qos;
      c.peer_qos = sub.qos_profile();
      c.direction = Direction::Publishes;
      c.type_match = (c.my_type == c.peer_type);
      // offered = publisher (self), requested = subscriber (peer)
      c.qos = check_qos_compat(my_qos.get_rmw_qos_profile(),
                               sub.qos_profile().get_rmw_qos_profile());
      view.publishes.push_back(std::move(c));
    }
  }

  // --- Topics this node SUBSCRIBES; peers are publishers. ---
  const auto sub_topics = graph->get_subscriber_names_and_types_by_node(name, ns);
  for (const auto & [topic, types] : sub_topics) {
    const std::string my_type = types.empty() ? std::string{} : types.front();

    const auto sub_infos = node.get_subscriptions_info_by_topic(topic);
    const rclcpp::QoS my_qos = find_self_qos(sub_infos, name, ns, rclcpp::QoS{10});

    for (const auto & pub : node.get_publishers_info_by_topic(topic)) {
      Connection c;
      c.topic = topic;
      c.self_node = view.name;
      c.peer_node = fq_name(pub.node_namespace(), pub.node_name());
      c.my_type = my_type;
      c.peer_type = pub.topic_type();
      c.my_qos = my_qos;
      c.peer_qos = pub.qos_profile();
      c.direction = Direction::Subscribes;
      c.type_match = (c.my_type == c.peer_type);
      // offered = publisher (peer), requested = subscriber (self)
      c.qos = check_qos_compat(pub.qos_profile().get_rmw_qos_profile(),
                               my_qos.get_rmw_qos_profile());
      view.subscribes.push_back(std::move(c));
    }
  }

  return view;
}

}  // namespace rni
