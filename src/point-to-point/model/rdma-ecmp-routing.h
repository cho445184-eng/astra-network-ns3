#ifndef RDMA_ECMP_ROUTING_H
#define RDMA_ECMP_ROUTING_H

#include <ns3/ipv4-routing-protocol.h>
#include <ns3/ipv4-address.h>
#include <ns3/node.h>
#include <unordered_map>
#include <vector>

namespace ns3 {

/**
 * \brief ECMP routing protocol for RDMA simulation.
 *
 * Wraps the existing per-destination ECMP hash routing used by
 * both RdmaHw (host) and SwitchNode into the standard
 * Ipv4RoutingProtocol interface.
 *
 * The routing table is a map from destination IP (uint32_t) to a
 * vector of egress interface indices. Lookups use a MurmurHash3-style
 * hash of the 5-tuple to select among equal-cost next hops.
 */
class RdmaEcmpRouting : public Ipv4RoutingProtocol {
public:
  static TypeId GetTypeId();
  RdmaEcmpRouting();
  ~RdmaEcmpRouting() override;

  // --- Ipv4RoutingProtocol interface ---
  Ptr<Ipv4Route> RouteOutput(Ptr<Packet> p,
                             const Ipv4Header &header,
                             Ptr<NetDevice> oif,
                             Socket::SocketErrno &sockerr) override;

  bool RouteInput(Ptr<const Packet> p,
                  const Ipv4Header &header,
                  Ptr<const NetDevice> idev,
                  const UnicastForwardCallback &ucb,
                  const MulticastForwardCallback &mcb,
                  const LocalDeliverCallback &lcb,
                  const ErrorCallback &ecb) override;

  void NotifyInterfaceUp(uint32_t interface) override;
  void NotifyInterfaceDown(uint32_t interface) override;
  void NotifyAddAddress(uint32_t interface, Ipv4InterfaceAddress address) override;
  void NotifyRemoveAddress(uint32_t interface, Ipv4InterfaceAddress address) override;
  void SetIpv4(Ptr<Ipv4> ipv4) override;
  void PrintRoutingTable(Ptr<OutputStreamWrapper> stream, Time::Unit unit = Time::S) const override;

  // --- ECMP table management ---
  void AddRoute(Ipv4Address dest, uint32_t interfaceIndex);
  void AddRoute(uint32_t destIp, uint32_t interfaceIndex);
  void ClearRoutes();

  /**
   * Lookup the egress interface for a given flow.
   * Uses ECMP hash on (sip, dip, sport, dport) to select among nexthops.
   */
  int LookupEcmp(uint32_t sip, uint32_t dip, uint16_t sport, uint16_t dport) const;

  void SetEcmpSeed(uint32_t seed);
  uint32_t GetEcmpSeed() const { return m_ecmpSeed; }

  const std::unordered_map<uint32_t, std::vector<int>>& GetRouteTable() const { return m_rtTable; }

  static uint32_t EcmpHash(const uint8_t* key, size_t len, uint32_t seed);

private:
  Ptr<Ipv4> m_ipv4;
  uint32_t m_ecmpSeed;
  std::unordered_map<uint32_t, std::vector<int>> m_rtTable;
};

} // namespace ns3

#endif // RDMA_ECMP_ROUTING_H
