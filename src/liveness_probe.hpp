#pragma once

#include <deque>
#include <map>
#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"

#include "graph_model.hpp"

namespace ci {

// Pure trailing-window rate estimator. Records arrival timestamps (seconds) and
// reports Hz over a trailing window. "known" becomes true once a full window has
// elapsed since probing started, so callers can distinguish Unknown from Dead.
class RateWindow {
public:
  explicit RateWindow(double window_s = 5.0) : window_s_(window_s) {}

  void start(double now_sec) { start_ = now_sec; started_ = true; }

  void record(double t_sec)
  {
    stamps_.push_back(t_sec);
    prune(t_sec);
  }

  // Arrivals within the trailing window divided by the window length.
  double hz(double now_sec) const
  {
    size_t n = 0;
    const double cutoff = now_sec - window_s_;
    for (auto it = stamps_.rbegin(); it != stamps_.rend() && *it > cutoff; ++it) {
      ++n;
    }
    return static_cast<double>(n) / window_s_;
  }

  // True once a full window has elapsed since start(): only then is hz==0
  // meaningful as "dead" rather than "still measuring".
  bool known(double now_sec) const
  {
    return started_ && (now_sec - start_) >= window_s_;
  }

private:
  void prune(double now_sec)
  {
    const double cutoff = now_sec - window_s_;
    while (!stamps_.empty() && stamps_.front() <= cutoff) {
      stamps_.pop_front();
    }
  }

  double window_s_;
  double start_ = 0.0;
  bool started_ = false;
  std::deque<double> stamps_;
};

// Manages short-lived generic subscriptions for the currently selected node.
// One probe per unique topic, adopting the PUBLISHER's offered QoS so the probe
// is guaranteed RxO-compatible. Payloads are never deserialized — only arrival
// timestamps are recorded.
class LivenessProbe {
public:
  explicit LivenessProbe(double window_s = 5.0) : window_s_(window_s) {}

  // Tear down all probes that aren't in `view`, create probes for new topics.
  void reconfigure(rclcpp::Node & node, const NodeView & view);

  // Destroy every probe (e.g. on selection change to a node with no topics).
  void clear() { probes_.clear(); }

  // Write measured hz / hz_known into the view's connections, keyed by topic.
  void apply(NodeView & view) const;

private:
  struct Probe {
    rclcpp::GenericSubscription::SharedPtr sub;
    std::shared_ptr<RateWindow> window;
  };

  // Publisher-side type + QoS for a connection (what a probe must adopt).
  static std::string probe_type(const Connection & c) { return c.pub_type(); }
  static const rclcpp::QoS & probe_qos(const Connection & c)
  {
    return c.direction == Direction::Publishes ? c.my_qos : c.peer_qos;
  }

  double window_s_;
  std::map<std::string, Probe> probes_;  // keyed by topic name
};

}  // namespace ci
