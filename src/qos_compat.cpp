#include "qos_compat.hpp"

#include <cstdint>
#include <string>

#include "rmw/types.h"

namespace rni {
namespace {

// --- Reliability -----------------------------------------------------------
// Rank: BEST_EFFORT < RELIABLE. Offered rank >= requested rank => OK.
// SYSTEM_DEFAULT / UNKNOWN / BEST_AVAILABLE: not enough info to flag => OK.
int reliability_rank(rmw_qos_reliability_policy_t p)
{
  switch (p) {
    case RMW_QOS_POLICY_RELIABILITY_BEST_EFFORT: return 0;
    case RMW_QOS_POLICY_RELIABILITY_RELIABLE:    return 1;
    default:                                     return -1;  // unconstrained
  }
}

const char * reliability_str(rmw_qos_reliability_policy_t p)
{
  switch (p) {
    case RMW_QOS_POLICY_RELIABILITY_BEST_EFFORT: return "BEST_EFFORT";
    case RMW_QOS_POLICY_RELIABILITY_RELIABLE:    return "RELIABLE";
    default:                                     return "SYSTEM_DEFAULT";
  }
}

// --- Durability ------------------------------------------------------------
// Rank: VOLATILE < TRANSIENT_LOCAL. Offered rank >= requested rank => OK.
int durability_rank(rmw_qos_durability_policy_t p)
{
  switch (p) {
    case RMW_QOS_POLICY_DURABILITY_VOLATILE:        return 0;
    case RMW_QOS_POLICY_DURABILITY_TRANSIENT_LOCAL: return 1;
    default:                                        return -1;  // unconstrained
  }
}

const char * durability_str(rmw_qos_durability_policy_t p)
{
  switch (p) {
    case RMW_QOS_POLICY_DURABILITY_VOLATILE:        return "VOLATILE";
    case RMW_QOS_POLICY_DURABILITY_TRANSIENT_LOCAL: return "TRANSIENT_LOCAL";
    default:                                        return "SYSTEM_DEFAULT";
  }
}

// --- Liveliness kind -------------------------------------------------------
// Rank: AUTOMATIC < MANUAL_BY_TOPIC. Offered rank >= requested rank => OK.
int liveliness_rank(rmw_qos_liveliness_policy_t p)
{
  switch (p) {
    case RMW_QOS_POLICY_LIVELINESS_AUTOMATIC:       return 0;
    case RMW_QOS_POLICY_LIVELINESS_MANUAL_BY_TOPIC: return 1;
    default:                                        return -1;  // unconstrained
  }
}

const char * liveliness_str(rmw_qos_liveliness_policy_t p)
{
  switch (p) {
    case RMW_QOS_POLICY_LIVELINESS_AUTOMATIC:       return "AUTOMATIC";
    case RMW_QOS_POLICY_LIVELINESS_MANUAL_BY_TOPIC: return "MANUAL_BY_TOPIC";
    default:                                        return "SYSTEM_DEFAULT";
  }
}

// --- Durations -------------------------------------------------------------
// {0,0} (unspecified) and the RMW infinite sentinel both mean "no constraint".
constexpr uint64_t kNsPerSec = 1000000000ULL;

bool is_infinite(const rmw_time_t & t)
{
  if (t.sec == 0 && t.nsec == 0) {
    return true;  // unspecified == infinite for deadline / lease_duration
  }
  // RMW_DURATION_INFINITE
  return t.sec == 9223372036ULL && t.nsec == 854775807ULL;
}

uint64_t to_ns(const rmw_time_t & t)
{
  return t.sec * kNsPerSec + t.nsec;
}

std::string duration_str(const rmw_time_t & t)
{
  if (is_infinite(t)) {
    return "default (infinite)";
  }
  const uint64_t ns = to_ns(t);
  if (ns % kNsPerSec == 0) {
    return std::to_string(ns / kNsPerSec) + "s";
  }
  return std::to_string(ns / 1000000ULL) + "ms";
}

}  // namespace

QosVerdict check_qos_compat(const rmw_qos_profile_t & offered,
                            const rmw_qos_profile_t & requested)
{
  // 1. Reliability
  {
    const int o = reliability_rank(offered.reliability);
    const int r = reliability_rank(requested.reliability);
    if (o >= 0 && r >= 0 && o < r) {
      return {false, "reliability",
              reliability_str(offered.reliability),
              reliability_str(requested.reliability)};
    }
  }

  // 2. Durability
  {
    const int o = durability_rank(offered.durability);
    const int r = durability_rank(requested.durability);
    if (o >= 0 && r >= 0 && o < r) {
      return {false, "durability",
              durability_str(offered.durability),
              durability_str(requested.durability)};
    }
  }

  // 3. Liveliness kind
  {
    const int o = liveliness_rank(offered.liveliness);
    const int r = liveliness_rank(requested.liveliness);
    if (o >= 0 && r >= 0 && o < r) {
      return {false, "liveliness",
              liveliness_str(offered.liveliness),
              liveliness_str(requested.liveliness)};
    }
  }

  // 4. Deadline: offered period <= requested period => OK.
  if (!is_infinite(requested.deadline)) {
    if (is_infinite(offered.deadline) || to_ns(offered.deadline) > to_ns(requested.deadline)) {
      return {false, "deadline",
              duration_str(offered.deadline),
              duration_str(requested.deadline)};
    }
  }

  // 5. Liveliness lease_duration: offered <= requested => OK.
  if (!is_infinite(requested.liveliness_lease_duration)) {
    if (is_infinite(offered.liveliness_lease_duration) ||
        to_ns(offered.liveliness_lease_duration) > to_ns(requested.liveliness_lease_duration))
    {
      return {false, "liveliness lease_duration",
              duration_str(offered.liveliness_lease_duration),
              duration_str(requested.liveliness_lease_duration)};
    }
  }

  return {true, "", "", ""};
}

}  // namespace rni
