#include "rdma-ecmp-routing.h"
#include "ns3/log.h"
#include "ns3/ipv4.h"
#include "ns3/ipv4-route.h"
#include "ns3/net-device.h"
#include "ns3/node.h"
#include "ns3/uinteger.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("RdmaEcmpRouting");
NS_OBJECT_ENSURE_REGISTERED(RdmaEcmpRouting);

TypeId RdmaEcmpRouting::GetTypeId() {
  static TypeId tid = TypeId("ns3::RdmaEcmpRouting")
    .SetParent<Ipv4RoutingProtocol>()
    .SetGroupName("PointToPoint")
    .AddConstructor<RdmaEcmpRouting>()
    .AddAttribute("EcmpSeed", "Seed for ECMP hash",
        UintegerValue(0),
        MakeUintegerAccessor(&RdmaEcmpRouting::m_ecmpSeed),
        MakeUintegerChecker<uint32_t>());
  return tid;
}

RdmaEcmpRouting::RdmaEcmpRouting() : m_ecmpSeed(0) {}
RdmaEcmpRouting::~RdmaEcmpRouting() {}

void RdmaEcmpRouting::SetIpv4(Ptr<Ipv4> ipv4) {
  m_ipv4 = ipv4;
}

void RdmaEcmpRouting::SetEcmpSeed(uint32_t seed) {
  m_ecmpSeed = seed;
}

void RdmaEcmpRouting::AddRoute(Ipv4Address dest, uint32_t interfaceIndex) {
  AddRoute(dest.Get(), interfaceIndex);
}

void RdmaEcmpRouting::AddRoute(uint32_t destIp, uint32_t interfaceIndex) {
  m_rtTable[destIp].push_back(interfaceIndex);
}

void RdmaEcmpRouting::ClearRoutes() {
  m_rtTable.clear();
}

int RdmaEcmpRouting::LookupEcmp(uint32_t sip, uint32_t dip,
                                 uint16_t sport, uint16_t dport) const {
  auto entry = m_rtTable.find(dip);
  if (entry == m_rtTable.end())
    return -1;
  auto &nexthops = entry->second;
  if (nexthops.empty())
    return -1;

  union {
    uint8_t u8[4 + 4 + 2 + 2];
    uint32_t u32[3];
  } buf;
  buf.u32[0] = sip;
  buf.u32[1] = dip;
  buf.u32[2] = sport | ((uint32_t)dport << 16);

  uint32_t idx = EcmpHash(buf.u8, 12, m_ecmpSeed) % nexthops.size();
  return nexthops[idx];
}

// MurmurHash3-style hash (identical to SwitchNode::EcmpHash)
uint32_t RdmaEcmpRouting::EcmpHash(const uint8_t* key, size_t len, uint32_t seed) {
  uint32_t h = seed;
  if (len > 3) {
    const uint32_t* key_x4 = (const uint32_t*)key;
    size_t i = len >> 2;
    do {
      uint32_t k = *key_x4++;
      k *= 0xcc9e2d51;
      k = (k << 15) | (k >> 17);
      k *= 0x1b873593;
      h ^= k;
      h = (h << 13) | (h >> 19);
      h += (h << 2) + 0xe6546b64;
    } while (--i);
    key = (const uint8_t*)key_x4;
  }
  if (len & 3) {
    size_t i = len & 3;
    uint32_t k = 0;
    key = &key[i - 1];
    do {
      k <<= 8;
      k |= *key--;
    } while (--i);
    k *= 0xcc9e2d51;
    k = (k << 15) | (k >> 17);
    k *= 0x1b873593;
    h ^= k;
  }
  h ^= len;
  h ^= h >> 16;
  h *= 0x85ebca6b;
  h ^= h >> 13;
  h *= 0xc2b2ae35;
  h ^= h >> 16;
  return h;
}

Ptr<Ipv4Route> RdmaEcmpRouting::RouteOutput(
    Ptr<Packet> p, const Ipv4Header &header,
    Ptr<NetDevice> oif, Socket::SocketErrno &sockerr) {
  // For host-side routing output, pick first available interface
  uint32_t dip = header.GetDestination().Get();
  auto entry = m_rtTable.find(dip);
  if (entry == m_rtTable.end() || entry->second.empty()) {
    sockerr = Socket::ERROR_NOROUTETOHOST;
    return nullptr;
  }
  Ptr<Ipv4Route> route = Create<Ipv4Route>();
  route->SetDestination(header.GetDestination());
  int ifIdx = entry->second[0]; // default to first next hop for output
  route->SetOutputDevice(m_ipv4->GetNetDevice(ifIdx));
  route->SetSource(m_ipv4->GetAddress(ifIdx, 0).GetLocal());
  route->SetGateway(Ipv4Address("0.0.0.0"));
  sockerr = Socket::ERROR_NOTERROR;
  return route;
}

bool RdmaEcmpRouting::RouteInput(
    Ptr<const Packet> p, const Ipv4Header &header,
    Ptr<const NetDevice> idev,
    const UnicastForwardCallback &ucb,
    const MulticastForwardCallback &mcb,
    const LocalDeliverCallback &lcb,
    const ErrorCallback &ecb) {
  uint32_t dip = header.GetDestination().Get();

  // Check if this packet is for a local address
  for (uint32_t i = 0; i < m_ipv4->GetNInterfaces(); i++) {
    for (uint32_t j = 0; j < m_ipv4->GetNAddresses(i); j++) {
      if (m_ipv4->GetAddress(i, j).GetLocal().Get() == dip) {
        uint32_t iif = m_ipv4->GetInterfaceForDevice(idev);
        lcb(p, header, iif);
        return true;
      }
    }
  }

  // Forward: look up ECMP route
  auto entry = m_rtTable.find(dip);
  if (entry == m_rtTable.end() || entry->second.empty()) {
    ecb(p, header, Socket::ERROR_NOROUTETOHOST);
    return false;
  }

  // Use ECMP hash with available header info
  uint16_t sport = 0, dport = 0;
  // Try to extract ports from packet for ECMP hash
  // (this is a best-effort; the actual RDMA path still uses SwitchNode directly)

  int ifIdx = LookupEcmp(header.GetSource().Get(), dip, sport, dport);
  if (ifIdx < 0) {
    ecb(p, header, Socket::ERROR_NOROUTETOHOST);
    return false;
  }

  Ptr<Ipv4Route> route = Create<Ipv4Route>();
  route->SetDestination(header.GetDestination());
  route->SetSource(header.GetSource());
  route->SetOutputDevice(m_ipv4->GetNetDevice(ifIdx));
  route->SetGateway(Ipv4Address("0.0.0.0"));

  ucb(route, p, header);
  return true;
}

void RdmaEcmpRouting::NotifyInterfaceUp(uint32_t interface) {}
void RdmaEcmpRouting::NotifyInterfaceDown(uint32_t interface) {}
void RdmaEcmpRouting::NotifyAddAddress(uint32_t interface, Ipv4InterfaceAddress address) {}
void RdmaEcmpRouting::NotifyRemoveAddress(uint32_t interface, Ipv4InterfaceAddress address) {}

void RdmaEcmpRouting::PrintRoutingTable(Ptr<OutputStreamWrapper> stream, Time::Unit unit) const {
  *stream->GetStream() << "RdmaEcmpRouting table (seed=" << m_ecmpSeed << "):\n";
  for (auto &entry : m_rtTable) {
    Ipv4Address addr(entry.first);
    *stream->GetStream() << "  " << addr << " -> [";
    for (size_t i = 0; i < entry.second.size(); i++) {
      if (i > 0) *stream->GetStream() << ", ";
      *stream->GetStream() << entry.second[i];
    }
    *stream->GetStream() << "]\n";
  }
}

} // namespace ns3
