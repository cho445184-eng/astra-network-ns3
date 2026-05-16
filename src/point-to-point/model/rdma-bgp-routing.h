#ifndef RDMA_BGP_ROUTING_H
#define RDMA_BGP_ROUTING_H

#include <ns3/ipv4-address.h>
#include <ns3/ipv4-routing-protocol.h>
#include <ns3/node.h>
#include <ns3/nstime.h>
#include <ns3/object.h>
#include <ns3/simulator.h>
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <unordered_map>
#include <vector>

namespace ns3 {

/**
 * \brief BGP path attributes for a single route entry.
 *
 * Models a simplified BGP UPDATE path attribute set:
 *   - AS_PATH:        vector of switch (AS) node IDs the route traverses
 *   - LOCAL_PREF:     higher is preferred (default 100)
 *   - MED:            lower is preferred (default 0, derived from link delay)
 *   - NEXT_HOP:       egress interface index on this node
 *   - ORIGIN:         originator node ID (the host advertising the prefix)
 */
struct BgpPathAttributes {
  std::vector<uint32_t> asPath;
  uint32_t localPref = 100;
  uint32_t med = 0;
  int nextHopInterface = -1;
  uint32_t originNodeId = 0;

  uint32_t AsPathLength() const { return asPath.size(); }

  bool ContainsAs(uint32_t asn) const {
    for (auto id : asPath)
      if (id == asn)
        return true;
    return false;
  }
};

/**
 * \brief A single entry in the BGP RIB (Routing Information Base).
 */
struct BgpRibEntry {
  uint32_t prefix;
  std::vector<BgpPathAttributes> paths;
  std::vector<int> bestNextHops;

  void SelectBestPaths();
};

/**
 * \brief BGP routing module for RDMA data center simulation.
 *
 * Implements a simplified BGP-like distributed routing protocol where each
 * switch acts as a BGP speaker (autonomous system). Routes propagate through
 * UPDATE messages scheduled via ns-3 Simulator events, modeling realistic
 * convergence behavior.
 *
 * Key features:
 *   - AS_PATH loop detection
 *   - Best-path selection: LOCAL_PREF > AS_PATH length > MED > router-id tiebreak
 *   - ECMP: all paths matching the best path's attributes are installed
 *   - Configurable route propagation delay
 *   - Withdraw handling on link failure
 *   - Integration with SwitchNode/RdmaHw routing tables
 *
 * Lifecycle:
 *   1. Host nodes originate routes for their own IP prefixes
 *   2. Switches receive and re-advertise routes to neighbors
 *   3. Best-path selection installs routes into m_rtTable for ECMP forwarding
 *   4. On link failure, withdrawals propagate and routes reconverge
 */
class RdmaBgpRouting : public Object {
public:
  static TypeId GetTypeId();
  RdmaBgpRouting();
  ~RdmaBgpRouting() override;

  using InstallRouteCallback =
      std::function<void(Ipv4Address prefix,
                         const std::vector<int> &nextHops)>;

  void SetNodeId(uint32_t id) { m_nodeId = id; }
  uint32_t GetNodeId() const { return m_nodeId; }

  void SetIsSwitch(bool isSwitch) { m_isSwitch = isSwitch; }

  /**
   * Register a neighbor reachable through the given interface.
   * \param neighborNodeId  The peer switch/host node ID
   * \param interfaceIdx    Local egress interface to reach this neighbor
   * \param linkDelay       One-way delay of the link (used for MED)
   */
  void AddNeighbor(uint32_t neighborNodeId, int interfaceIdx,
                   uint64_t linkDelay);

  /**
   * Originate a route for this node's prefix (called on host nodes).
   */
  void OriginateRoute(Ipv4Address prefix);

  /**
   * Receive a BGP UPDATE from a neighbor.
   * This is the core protocol entry point, scheduled as a simulator event.
   */
  void ReceiveUpdate(uint32_t fromNeighborId, uint32_t prefix,
                     BgpPathAttributes pathAttrs);

  /**
   * Withdraw a route learned from a neighbor (e.g. on link failure).
   */
  void WithdrawRoutesFromNeighbor(uint32_t neighborNodeId);

  /**
   * Set callback invoked when the best route for a prefix changes.
   * The callback should install/update the node's forwarding table.
   */
  void SetInstallRouteCallback(InstallRouteCallback cb) {
    m_installRouteCb = cb;
  }

  void SetPropagationDelay(Time delay) { m_propagationDelay = delay; }

  const std::map<uint32_t, BgpRibEntry> &GetRib() const { return m_rib; }

private:
  void AdvertiseToNeighbors(uint32_t prefix, const BgpPathAttributes &best);
  void UpdateForwardingTable(uint32_t prefix, BgpRibEntry &entry);

  uint32_t m_nodeId;
  bool m_isSwitch;
  Time m_propagationDelay;
  uint32_t m_defaultLocalPref;

  struct NeighborInfo {
    uint32_t nodeId;
    int interfaceIdx;
    uint64_t linkDelay;
  };
  std::vector<NeighborInfo> m_neighbors;

  std::map<uint32_t, BgpRibEntry> m_rib;

  using BgpPeerMap =
      std::unordered_map<uint32_t, RdmaBgpRouting *>;
  static BgpPeerMap &GetPeerMap() {
    static BgpPeerMap s_peerMap;
    return s_peerMap;
  }

  InstallRouteCallback m_installRouteCb;
};

} // namespace ns3

#endif // RDMA_BGP_ROUTING_H
