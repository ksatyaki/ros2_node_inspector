#include "status.hpp"

#include <sstream>
#include <string>

#include "graph_model.hpp"

namespace ci {

EdgeStatus compute_status(const Connection & c)
{
  if (!c.type_match) {
    return EdgeStatus::TypeMismatch;
  }
  if (!c.qos.compatible) {
    return EdgeStatus::QosMismatch;
  }
  if (c.latched()) {
    return EdgeStatus::Latched;  // retained value; rate is not meaningful
  }
  if (!c.hz_known) {
    return EdgeStatus::Unknown;
  }
  if (c.hz == 0.0) {
    return EdgeStatus::Dead;
  }
  return EdgeStatus::Ok;
}

namespace {
std::string fmt_hz(double hz)
{
  std::ostringstream os;
  os.precision(hz < 10.0 ? 1 : 0);
  os << std::fixed << hz;
  return os.str();
}
}  // namespace

std::string status_detail(const Connection & c)
{
  std::ostringstream os;
  switch (compute_status(c)) {
    case EdgeStatus::TypeMismatch:
      os << c.topic << ": " << c.pub_node() << " publishes " << c.pub_type()
         << ", " << c.sub_node() << " subscribes as " << c.sub_type();
      break;
    case EdgeStatus::QosMismatch:
      os << c.topic << ": publisher " << c.pub_node() << " offers " << c.qos.offered
         << ", subscriber " << c.sub_node() << " requests " << c.qos.requested
         << " → " << c.qos.policy << " incompatible";
      break;
    case EdgeStatus::Dead:
      os << c.topic << ": QoS+type OK but no data in last 5 s (0 Hz)";
      break;
    case EdgeStatus::Latched:
      os << c.topic << ": latched (TRANSIENT_LOCAL) — retained value, rate not monitored";
      break;
    case EdgeStatus::Ok:
      os << c.topic << ": live, " << fmt_hz(c.hz) << " Hz";
      break;
    case EdgeStatus::Unknown:
      os << c.topic << ": measuring… (probe window not yet elapsed)";
      break;
  }
  return os.str();
}

}  // namespace ci
