#pragma once

#include <string>

namespace rni {

struct Connection;  // defined in graph_model.hpp

// Composite per-edge status: exactly one icon per connection.
//   Ok          = green tick
//   QosMismatch = amber "?"
//   TypeMismatch / Dead = red cross
//   Unknown     = grey (liveness probe window not yet elapsed)
enum class EdgeStatus { Ok, QosMismatch, TypeMismatch, Dead, Unknown };

// Precedence (highest first):
//   1. !type_match               -> TypeMismatch
//   2. !qos.compatible           -> QosMismatch
//   3. hz_known && hz == 0       -> Dead
//   4. hz_known                  -> Ok
//   5. otherwise                 -> Unknown
EdgeStatus compute_status(const Connection & c);

// Plain-language popup text describing the connection's status.
std::string status_detail(const Connection & c);

}  // namespace rni
