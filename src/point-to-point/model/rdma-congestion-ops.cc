#include "rdma-congestion-ops.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("RdmaCongestionOps");
NS_OBJECT_ENSURE_REGISTERED(RdmaCongestionOps);

TypeId RdmaCongestionOps::GetTypeId() {
  static TypeId tid = TypeId("ns3::RdmaCongestionOps")
    .SetParent<Object>()
    .SetGroupName("PointToPoint");
  return tid;
}

RdmaCongestionOps::RdmaCongestionOps() : m_rdmaHw(nullptr) {}
RdmaCongestionOps::~RdmaCongestionOps() {}

void RdmaCongestionOps::SetRdmaHw(RdmaHw* hw) {
  m_rdmaHw = hw;
}

} // namespace ns3
