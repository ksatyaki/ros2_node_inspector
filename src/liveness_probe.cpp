#include "liveness_probe.hpp"

#include <chrono>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "rclcpp/rclcpp.hpp"

namespace ci {
namespace {

double steady_now_sec()
{
  using namespace std::chrono;
  return duration<double>(steady_clock::now().time_since_epoch()).count();
}

// A peer's offered QoS discovered via TopicEndpointInfo reports history as
// UNKNOWN with depth 0 — history is not propagated over DDS discovery and is
// not part of RxO compatibility. Normalise it to a valid KEEP_LAST so the probe
// subscription can be created, while preserving the discoverable policies
// (reliability/durability/deadline/liveliness) that keep the probe compatible.
rclcpp::QoS sanitize_probe_qos(const rclcpp::QoS & offered)
{
  rmw_qos_profile_t p = offered.get_rmw_qos_profile();
  if (p.history == RMW_QOS_POLICY_HISTORY_UNKNOWN ||
      p.history == RMW_QOS_POLICY_HISTORY_SYSTEM_DEFAULT)
  {
    p.history = RMW_QOS_POLICY_HISTORY_KEEP_LAST;
  }
  if (p.depth == 0) {
    p.depth = 10;
  }
  return rclcpp::QoS(rclcpp::QoSInitialization::from_rmw(p), p);
}

// Collect each unique topic the view touches, with the publisher-side type and
// QoS a probe must adopt. Both publish/subscribe sides are probed (we measure
// whether data is flowing on the topic regardless of direction).
struct ProbeSpec {
  std::string type;
  rclcpp::QoS qos{10};
};

std::map<std::string, ProbeSpec> collect_specs(const NodeView & view)
{
  std::map<std::string, ProbeSpec> specs;
  auto add = [&specs](const Connection & c) {
    if (c.topic.empty() || c.pub_type().empty()) {
      return;  // can't build a generic subscription without a concrete type
    }
    specs.emplace(c.topic, ProbeSpec{
      c.direction == Direction::Publishes ? c.my_type : c.peer_type,
      sanitize_probe_qos(c.direction == Direction::Publishes ? c.my_qos : c.peer_qos)});
  };
  for (const auto & c : view.publishes) add(c);
  for (const auto & c : view.subscribes) add(c);
  return specs;
}

}  // namespace

void LivenessProbe::reconfigure(rclcpp::Node & node, const NodeView & view)
{
  const auto specs = collect_specs(view);

  // Drop probes for topics no longer present.
  for (auto it = probes_.begin(); it != probes_.end();) {
    if (specs.find(it->first) == specs.end()) {
      it = probes_.erase(it);
    } else {
      ++it;
    }
  }

  // Create probes for newly added topics.
  for (const auto & [topic, spec] : specs) {
    if (probes_.find(topic) != probes_.end()) {
      continue;
    }
    auto window = std::make_shared<RateWindow>(window_s_);
    window->start(steady_now_sec());
    auto cb = [window](std::shared_ptr<rclcpp::SerializedMessage>) {
      // Never deserialize — record arrival time only.
      window->record(steady_now_sec());
    };
    Probe probe;
    probe.window = window;
    try {
      probe.sub = node.create_generic_subscription(topic, spec.type, spec.qos, cb);
    } catch (const std::exception & e) {
      // Exotic / unresolvable type+QoS: leave hz unknown (surfaces as Unknown).
      RCLCPP_WARN(node.get_logger(), "probe for '%s' (%s) failed: %s",
                  topic.c_str(), spec.type.c_str(), e.what());
      continue;
    }
    probes_.emplace(topic, std::move(probe));
  }
}

void LivenessProbe::apply(NodeView & view) const
{
  const double now = steady_now_sec();
  auto fill = [this, now](Connection & c) {
    auto it = probes_.find(c.topic);
    if (it == probes_.end()) {
      c.hz_known = false;
      c.hz = 0.0;
      return;
    }
    const auto & w = *it->second.window;
    c.hz_known = w.known(now);
    c.hz = w.hz(now);
  };
  for (auto & c : view.publishes) fill(c);
  for (auto & c : view.subscribes) fill(c);
}

}  // namespace ci
