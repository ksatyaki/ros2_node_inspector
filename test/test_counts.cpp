#include <gtest/gtest.h>

#include "graph_model.hpp"

using ci::Connection;
using ci::NodeView;

namespace {

Connection make(bool type_match, bool qos_ok)
{
  Connection c;
  c.type_match = type_match;
  c.qos.compatible = qos_ok;
  return c;
}

}  // namespace

TEST(Counts, ClassifiesEachConnectionByTypeThenQos)
{
  NodeView v;
  v.publishes.push_back(make(true, true));    // ok
  v.publishes.push_back(make(true, false));   // warn (qos)
  v.subscribes.push_back(make(false, true));  // err (type)
  v.subscribes.push_back(make(false, false)); // err (type dominates qos)

  auto counts = ci::count_connections(v);
  EXPECT_EQ(counts.ok, 1);
  EXPECT_EQ(counts.warn, 1);
  EXPECT_EQ(counts.err, 2);
}

TEST(Counts, LivenessIsExcludedFromCounts)
{
  NodeView v;
  // A dead-but-otherwise-healthy connection still counts as ok (type+QoS only).
  auto dead = make(true, true);
  dead.hz_known = true;
  dead.hz = 0.0;
  v.publishes.push_back(dead);

  auto counts = ci::count_connections(v);
  EXPECT_EQ(counts.ok, 1);
  EXPECT_EQ(counts.warn, 0);
  EXPECT_EQ(counts.err, 0);
}

TEST(Counts, EmptyViewIsAllZero)
{
  auto counts = ci::count_connections(NodeView{});
  EXPECT_EQ(counts.ok, 0);
  EXPECT_EQ(counts.warn, 0);
  EXPECT_EQ(counts.err, 0);
}
