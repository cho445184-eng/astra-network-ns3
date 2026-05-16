#ifndef RDMA_BGP_ROUTING_H
#define RDMA_BGP_ROUTING_H

#include <ns3/bgp.h>
#include <ns3/node.h>
#include <ns3/object.h>
#include <ns3/ipv4-address.h>
#include <ns3/nstime.h>
#include <ns3/simulator.h>
#include "switch-node.h"
#include "rdma-hw.h"
#include <ns3/rdma-driver.h>
#include <functional>
#include <map>
#include <vector>

namespace ns3 {

/**
 * \brief Bridge between Nat-Lab/ns3-bgp (libbgp-based BGP) and RDMA forwarding tables.
 *
 * The RDMA data path uses custom L2-level forwarding (SwitchNode::m_rtTable,
 * RdmaHw::m_rtTable) that bypasses the standard Ipv4RoutingProtocol stack.
 * This bridge subscribes to the libbgp RouteEventBus and pushes learned BGP
 * routes into the RDMA forwarding tables so that RDMA traffic follows
 * BGP-computed paths.
 *
 * Usage:
 *   1. Install ns3::Bgp application on each node
 *   2. Create RdmaBgpBridge per node, call SetNode() and SetBgpApp()
 *   3. Call Start() after BGP application is started
 *   4. The bridge polls the BGP RIB periodically and syncs changes
 *      to the RDMA forwarding tables
 */
class RdmaBgpBridge : public Object {
public:
  static TypeId GetTypeId();
  RdmaBgpBridge();

  void SetNode(Ptr<Node> node);
  void SetBgpApp(Ptr<Bgp> bgp);
  void SetSyncInterval(Time interval);
  void Start();

private:
  void SyncRoutes();

  Ptr<Node> m_node;
  Ptr<Bgp> m_bgpApp;
  Time m_syncInterval;
  bool m_running;

  struct RibSnapshot {
    std::map<uint32_t, uint32_t> prefixToNexthop;
  };
  RibSnapshot m_lastSnapshot;
};

} // namespace ns3

#endif // RDMA_BGP_ROUTING_H
