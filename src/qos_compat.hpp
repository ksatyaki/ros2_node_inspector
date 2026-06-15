#pragma once

#include <string>

#include "rmw/types.h"

namespace ci {

// Result of a Request-vs-Offered QoS compatibility check.
// Carries the FIRST failing policy and its offered/requested values so the
// status popup can build the exact human sentence. No partial state.
struct QosVerdict {
  bool compatible = true;
  std::string policy;     // e.g. "durability"  (empty if compatible)
  std::string offered;    // e.g. "TRANSIENT_LOCAL"
  std::string requested;  // e.g. "VOLATILE"
};

// RxO compatibility: publisher provides the OFFERED profile, subscriber the
// REQUESTED profile. Flags incompatible only on a true RxO violation, never on
// a mere difference. Checks policies in this order and returns on the first
// violation: reliability, durability, liveliness kind, deadline,
// liveliness lease_duration. History depth is never an incompatibility.
QosVerdict check_qos_compat(const rmw_qos_profile_t & offered,
                            const rmw_qos_profile_t & requested);

}  // namespace ci
