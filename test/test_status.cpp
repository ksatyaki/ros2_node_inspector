#include <gtest/gtest.h>

#include "graph_model.hpp"
#include "status.hpp"

using rni::Connection;
using rni::Direction;
using rni::EdgeStatus;

namespace {

// A fully-healthy connection: type matches, QoS compatible, live.
Connection healthy()
{
  Connection c;
  c.topic = "/map";
  c.self_node = "/node_a";
  c.peer_node = "/node_b";
  c.my_type = "nav_msgs/msg/OccupancyGrid";
  c.peer_type = "nav_msgs/msg/OccupancyGrid";
  c.direction = Direction::Publishes;
  c.type_match = true;
  c.qos = {true, "", "", ""};
  c.hz_known = true;
  c.hz = 30.0;
  return c;
}

}  // namespace

TEST(Status, OkWhenAllHealthy)
{
  EXPECT_EQ(healthy().status(), EdgeStatus::Ok);
}

TEST(Status, TypeMismatchBeatsQosMismatch)
{
  auto c = healthy();
  c.type_match = false;
  c.qos = {false, "durability", "VOLATILE", "TRANSIENT_LOCAL"};
  EXPECT_EQ(c.status(), EdgeStatus::TypeMismatch);
}

TEST(Status, QosMismatchBeatsDead)
{
  auto c = healthy();
  c.qos = {false, "reliability", "BEST_EFFORT", "RELIABLE"};
  c.hz_known = true;
  c.hz = 0.0;  // would be Dead, but QoS dominates
  EXPECT_EQ(c.status(), EdgeStatus::QosMismatch);
}

TEST(Status, DeadWhenTypeAndQosOkButNoData)
{
  auto c = healthy();
  c.hz_known = true;
  c.hz = 0.0;
  EXPECT_EQ(c.status(), EdgeStatus::Dead);
}

TEST(Status, UnknownBeforeProbeWindowElapses)
{
  auto c = healthy();
  c.hz_known = false;
  c.hz = 0.0;
  EXPECT_EQ(c.status(), EdgeStatus::Unknown);
}

TEST(Status, QosMismatchPopupNamesPubSubAndPolicy)
{
  auto c = healthy();
  c.direction = Direction::Publishes;  // self publishes, peer subscribes
  c.qos = {false, "durability", "TRANSIENT_LOCAL", "VOLATILE"};
  const std::string s = rni::status_detail(c);
  EXPECT_NE(s.find("publisher /node_a offers TRANSIENT_LOCAL"), std::string::npos);
  EXPECT_NE(s.find("subscriber /node_b requests VOLATILE"), std::string::npos);
  EXPECT_NE(s.find("durability incompatible"), std::string::npos);
}

TEST(Status, TypeMismatchPopupNamesBothTypes)
{
  auto c = healthy();
  c.direction = Direction::Subscribes;  // self subscribes, peer publishes
  c.my_type = "std_msgs/msg/String";
  c.peer_type = "std_msgs/msg/Int32";
  c.type_match = false;
  const std::string s = rni::status_detail(c);
  // peer is the publisher here
  EXPECT_NE(s.find("/node_b publishes std_msgs/msg/Int32"), std::string::npos);
  EXPECT_NE(s.find("/node_a subscribes as std_msgs/msg/String"), std::string::npos);
}
