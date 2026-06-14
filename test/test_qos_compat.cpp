#include <gtest/gtest.h>

#include "rmw/qos_profiles.h"
#include "rmw/types.h"

#include "qos_compat.hpp"

using rni::check_qos_compat;

namespace {

rmw_qos_profile_t base()
{
  // Sensible, fully-compatible starting point; tests perturb one policy.
  rmw_qos_profile_t p = rmw_qos_profile_default;
  p.reliability = RMW_QOS_POLICY_RELIABILITY_RELIABLE;
  p.durability = RMW_QOS_POLICY_DURABILITY_VOLATILE;
  p.liveliness = RMW_QOS_POLICY_LIVELINESS_AUTOMATIC;
  p.deadline = RMW_DURATION_INFINITE;
  p.liveliness_lease_duration = RMW_DURATION_INFINITE;
  return p;
}

}  // namespace

TEST(QosCompat, ReliabilityBestEffortPubToReliableSubIncompatible)
{
  auto offered = base();
  offered.reliability = RMW_QOS_POLICY_RELIABILITY_BEST_EFFORT;
  auto requested = base();
  requested.reliability = RMW_QOS_POLICY_RELIABILITY_RELIABLE;

  auto v = check_qos_compat(offered, requested);
  EXPECT_FALSE(v.compatible);
  EXPECT_EQ(v.policy, "reliability");
  EXPECT_EQ(v.offered, "BEST_EFFORT");
  EXPECT_EQ(v.requested, "RELIABLE");
}

TEST(QosCompat, ReliabilityReliablePubToBestEffortSubOk)
{
  auto offered = base();
  offered.reliability = RMW_QOS_POLICY_RELIABILITY_RELIABLE;
  auto requested = base();
  requested.reliability = RMW_QOS_POLICY_RELIABILITY_BEST_EFFORT;

  EXPECT_TRUE(check_qos_compat(offered, requested).compatible);
}

TEST(QosCompat, DurabilityVolatilePubToTransientLocalSubIncompatible)
{
  auto offered = base();
  offered.durability = RMW_QOS_POLICY_DURABILITY_VOLATILE;
  auto requested = base();
  requested.durability = RMW_QOS_POLICY_DURABILITY_TRANSIENT_LOCAL;

  auto v = check_qos_compat(offered, requested);
  EXPECT_FALSE(v.compatible);
  EXPECT_EQ(v.policy, "durability");
  EXPECT_EQ(v.offered, "VOLATILE");
  EXPECT_EQ(v.requested, "TRANSIENT_LOCAL");
}

TEST(QosCompat, DurabilityTransientLocalPubToVolatileSubOk)
{
  auto offered = base();
  offered.durability = RMW_QOS_POLICY_DURABILITY_TRANSIENT_LOCAL;
  auto requested = base();
  requested.durability = RMW_QOS_POLICY_DURABILITY_VOLATILE;

  EXPECT_TRUE(check_qos_compat(offered, requested).compatible);
}

TEST(QosCompat, LivelinessManualPubSatisfiesAutomaticSub)
{
  auto offered = base();
  offered.liveliness = RMW_QOS_POLICY_LIVELINESS_MANUAL_BY_TOPIC;
  auto requested = base();
  requested.liveliness = RMW_QOS_POLICY_LIVELINESS_AUTOMATIC;

  EXPECT_TRUE(check_qos_compat(offered, requested).compatible);
}

TEST(QosCompat, LivelinessAutomaticPubToManualSubIncompatible)
{
  auto offered = base();
  offered.liveliness = RMW_QOS_POLICY_LIVELINESS_AUTOMATIC;
  auto requested = base();
  requested.liveliness = RMW_QOS_POLICY_LIVELINESS_MANUAL_BY_TOPIC;

  auto v = check_qos_compat(offered, requested);
  EXPECT_FALSE(v.compatible);
  EXPECT_EQ(v.policy, "liveliness");
}

TEST(QosCompat, DeadlineOfferedLongerThanRequestedIncompatible)
{
  auto offered = base();
  offered.deadline = rmw_time_t{1, 0};   // 1 s period
  auto requested = base();
  requested.deadline = rmw_time_t{0, 500000000};  // 500 ms — stricter

  auto v = check_qos_compat(offered, requested);
  EXPECT_FALSE(v.compatible);
  EXPECT_EQ(v.policy, "deadline");
}

TEST(QosCompat, DeadlineOfferedStricterThanRequestedOk)
{
  auto offered = base();
  offered.deadline = rmw_time_t{0, 500000000};  // 500 ms
  auto requested = base();
  requested.deadline = rmw_time_t{1, 0};         // 1 s — looser

  EXPECT_TRUE(check_qos_compat(offered, requested).compatible);
}

TEST(QosCompat, DefaultsAreCompatible)
{
  EXPECT_TRUE(check_qos_compat(base(), base()).compatible);
}

TEST(QosCompat, ReliabilityCheckedBeforeDurability)
{
  // Both reliability and durability violate; reliability is reported first.
  auto offered = base();
  offered.reliability = RMW_QOS_POLICY_RELIABILITY_BEST_EFFORT;
  offered.durability = RMW_QOS_POLICY_DURABILITY_VOLATILE;
  auto requested = base();
  requested.reliability = RMW_QOS_POLICY_RELIABILITY_RELIABLE;
  requested.durability = RMW_QOS_POLICY_DURABILITY_TRANSIENT_LOCAL;

  EXPECT_EQ(check_qos_compat(offered, requested).policy, "reliability");
}
