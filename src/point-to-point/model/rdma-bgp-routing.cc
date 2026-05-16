#include "rdma-bgp-routing.h"
#include "ns3/log.h"
#include "ns3/uinteger.h"
#include "ns3/ipv4.h"
#include "qbb-net-device.h"
#include <arpa/inet.h>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("RdmaBgpBridge");
NS_OBJECT_ENSURE_REGISTERED(RdmaBgpBridge);

TypeId RdmaBgpBridge::GetTypeId() {
  static TypeId tid = TypeId("ns3::RdmaBgpBridge")
    .SetParent<Object>()
    .SetGroupName("PointToPoint")
    .AddConstructor<RdmaBgpBridge>();
  return tid;
}

RdmaBgpBridge::RdmaBgpBridge()
    : m_syncInterval(MilliSeconds(10)), m_running(false) {}

void RdmaBgpBridge::SetNode(Ptr<Node> node) { m_node = node; }
void RdmaBgpBridge::SetBgpApp(Ptr<Bgp> bgp) { m_bgpApp = bgp; }
void RdmaBgpBridge::SetSyncInterval(Time interval) { m_syncInterval = interval; }

void RdmaBgpBridge::Start() {
  m_running = true;
  Simulator::Schedule(m_syncInterval, &RdmaBgpBridge::SyncRoutes, this);
}

/**
 * Periodically scan the libbgp RIB and push any changes into the
 * RDMA forwarding tables (SwitchNode::m_rtTable or RdmaHw::m_rtTable).
 *
 * The RDMA forwarding tables map destination IP (uint32_t host-byte-order)
 * to a vector of egress interface indices. The libbgp RIB stores nexthop
 * as an IP address in network byte order. We convert the nexthop IP to
 * the local egress interface index by matching against interface addresses.
 */
void RdmaBgpBridge::SyncRoutes() {
  if (!m_running || !m_bgpApp || !m_node)
    return;

  const libbgp::BgpRib4 &rib = m_bgpApp->GetRib();
  Ptr<Ipv4> ipv4 = m_node->GetObject<Ipv4>();
  if (!ipv4) {
    Simulator::Schedule(m_syncInterval, &RdmaBgpBridge::SyncRoutes, this);
    return;
  }

  RibSnapshot current;
  for (const auto &entry_pair : rib.get()) {
    const libbgp::BgpRib4Entry &entry = entry_pair.second;
    uint32_t prefix_net = entry.route.getPrefix();
    uint32_t nexthop_net = entry.getNexthop();
    uint32_t prefix_host = ntohl(prefix_net);
    uint32_t nexthop_host = ntohl(nexthop_net);
    current.prefixToNexthop[prefix_host] = nexthop_host;
  }

  if (current.prefixToNexthop != m_lastSnapshot.prefixToNexthop) {
    NS_LOG_DEBUG("Node " << m_node->GetId() << " BGP RIB changed, syncing "
                         << current.prefixToNexthop.size() << " routes to RDMA tables");

    auto resolveNexthopToInterface = [&](uint32_t nexthopHost) -> int {
      Ipv4Address nhAddr(nexthopHost);
      for (uint32_t i = 0; i < ipv4->GetNInterfaces(); i++) {
        for (uint32_t j = 0; j < ipv4->GetNAddresses(i); j++) {
          Ipv4InterfaceAddress addr = ipv4->GetAddress(i, j);
          if (addr.GetLocal().CombineMask(addr.GetMask()) ==
              nhAddr.CombineMask(addr.GetMask())) {
            if (ipv4->IsUp(i))
              return static_cast<int>(i);
          }
        }
      }
      return -1;
    };

    if (m_node->GetNodeType() == 1) {
      Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(m_node);
      sw->ClearTable();
      for (auto &[pfxHost, nhHost] : current.prefixToNexthop) {
        int ifIdx = resolveNexthopToInterface(nhHost);
        if (ifIdx >= 0) {
          Ipv4Address pfxAddr(pfxHost);
          sw->AddTableEntry(pfxAddr, ifIdx);
        }
      }
    } else {
      Ptr<RdmaDriver> rdma = m_node->GetObject<RdmaDriver>();
      if (rdma && rdma->m_rdma) {
        rdma->m_rdma->ClearTable();
        for (auto &[pfxHost, nhHost] : current.prefixToNexthop) {
          int ifIdx = resolveNexthopToInterface(nhHost);
          if (ifIdx >= 0) {
            Ipv4Address pfxAddr(pfxHost);
            rdma->m_rdma->AddTableEntry(pfxAddr, ifIdx);
          }
        }
      }
    }

    m_lastSnapshot = current;
  }

  Simulator::Schedule(m_syncInterval, &RdmaBgpBridge::SyncRoutes, this);
}

} // namespace ns3
