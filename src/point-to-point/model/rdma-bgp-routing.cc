#include "rdma-bgp-routing.h"
#include "ns3/log.h"
#include "ns3/uinteger.h"
#include "ns3/boolean.h"
#include "ns3/double.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("RdmaBgpRouting");
NS_OBJECT_ENSURE_REGISTERED(RdmaBgpRouting);

// --- BgpRibEntry ---

void BgpRibEntry::SelectBestPaths() {
  bestNextHops.clear();
  if (paths.empty())
    return;

  // Phase 1: find the best path attributes using BGP decision process
  const BgpPathAttributes *best = nullptr;
  for (auto &p : paths) {
    if (!best) {
      best = &p;
      continue;
    }
    // 1) Highest LOCAL_PREF
    if (p.localPref > best->localPref) {
      best = &p;
      continue;
    }
    if (p.localPref < best->localPref)
      continue;
    // 2) Shortest AS_PATH
    if (p.AsPathLength() < best->AsPathLength()) {
      best = &p;
      continue;
    }
    if (p.AsPathLength() > best->AsPathLength())
      continue;
    // 3) Lowest MED
    if (p.med < best->med) {
      best = &p;
      continue;
    }
    if (p.med > best->med)
      continue;
    // 4) Lowest origin node ID (tiebreak)
    if (p.originNodeId < best->originNodeId) {
      best = &p;
      continue;
    }
  }

  if (!best)
    return;

  // Phase 2: collect all ECMP paths matching the best attributes
  std::set<int> seen;
  for (auto &p : paths) {
    if (p.localPref == best->localPref &&
        p.AsPathLength() == best->AsPathLength() && p.med == best->med &&
        p.nextHopInterface >= 0) {
      if (seen.insert(p.nextHopInterface).second)
        bestNextHops.push_back(p.nextHopInterface);
    }
  }
}

// --- RdmaBgpRouting ---

TypeId RdmaBgpRouting::GetTypeId() {
  static TypeId tid =
      TypeId("ns3::RdmaBgpRouting")
          .SetParent<Object>()
          .SetGroupName("PointToPoint")
          .AddConstructor<RdmaBgpRouting>()
          .AddAttribute("DefaultLocalPref",
                        "Default LOCAL_PREF for originated routes",
                        UintegerValue(100),
                        MakeUintegerAccessor(
                            &RdmaBgpRouting::m_defaultLocalPref),
                        MakeUintegerChecker<uint32_t>());
  return tid;
}

RdmaBgpRouting::RdmaBgpRouting()
    : m_nodeId(0), m_isSwitch(false), m_propagationDelay(MicroSeconds(1)),
      m_defaultLocalPref(100) {}

RdmaBgpRouting::~RdmaBgpRouting() { GetPeerMap().erase(m_nodeId); }

void RdmaBgpRouting::AddNeighbor(uint32_t neighborNodeId, int interfaceIdx,
                                 uint64_t linkDelay) {
  m_neighbors.push_back({neighborNodeId, interfaceIdx, linkDelay});
  GetPeerMap()[m_nodeId] = this;
}

void RdmaBgpRouting::OriginateRoute(Ipv4Address prefix) {
  uint32_t pfx = prefix.Get();

  BgpPathAttributes attrs;
  attrs.localPref = m_defaultLocalPref;
  attrs.med = 0;
  attrs.originNodeId = m_nodeId;
  attrs.nextHopInterface = -1;

  auto &entry = m_rib[pfx];
  entry.prefix = pfx;
  entry.paths.clear();
  entry.paths.push_back(attrs);
  entry.SelectBestPaths();

  for (auto &nbr : m_neighbors) {
    BgpPathAttributes adv = attrs;
    adv.nextHopInterface = -1;
    auto *peer = GetPeerMap().count(nbr.nodeId)
                     ? GetPeerMap()[nbr.nodeId]
                     : nullptr;
    if (peer) {
      adv.nextHopInterface = nbr.interfaceIdx;
      Simulator::Schedule(m_propagationDelay,
                          &RdmaBgpRouting::ReceiveUpdate, peer, m_nodeId,
                          pfx, adv);
    }
  }

  NS_LOG_DEBUG("Node " << m_nodeId << " originated route for "
                        << prefix);
}

void RdmaBgpRouting::ReceiveUpdate(uint32_t fromNeighborId, uint32_t prefix,
                                   BgpPathAttributes pathAttrs) {
  if (pathAttrs.ContainsAs(m_nodeId)) {
    NS_LOG_DEBUG("Node " << m_nodeId << " dropping UPDATE for "
                          << Ipv4Address(prefix)
                          << " (AS-path loop, already contains "
                          << m_nodeId << ")");
    return;
  }

  int ingressInterface = -1;
  uint64_t linkDelay = 0;
  for (auto &nbr : m_neighbors) {
    if (nbr.nodeId == fromNeighborId) {
      ingressInterface = nbr.interfaceIdx;
      linkDelay = nbr.linkDelay;
      break;
    }
  }
  if (ingressInterface < 0) {
    NS_LOG_WARN("Node " << m_nodeId
                         << " received UPDATE from unknown neighbor "
                         << fromNeighborId);
    return;
  }

  BgpPathAttributes localAttrs = pathAttrs;
  localAttrs.asPath.push_back(m_nodeId);
  localAttrs.nextHopInterface = ingressInterface;
  localAttrs.med += static_cast<uint32_t>(linkDelay / 1000);

  auto &entry = m_rib[prefix];
  entry.prefix = prefix;

  bool replaced = false;
  for (auto &existing : entry.paths) {
    if (existing.nextHopInterface == ingressInterface &&
        existing.originNodeId == localAttrs.originNodeId) {
      existing = localAttrs;
      replaced = true;
      break;
    }
  }
  if (!replaced)
    entry.paths.push_back(localAttrs);

  auto oldBest = entry.bestNextHops;
  entry.SelectBestPaths();

  if (entry.bestNextHops != oldBest) {
    UpdateForwardingTable(prefix, entry);

    if (m_isSwitch && !entry.bestNextHops.empty()) {
      BgpPathAttributes bestForAdv;
      for (auto &p : entry.paths) {
        if (!entry.bestNextHops.empty() &&
            p.nextHopInterface == entry.bestNextHops[0]) {
          bestForAdv = p;
          break;
        }
      }
      AdvertiseToNeighbors(prefix, bestForAdv);
    }
  }

  NS_LOG_DEBUG("Node " << m_nodeId << " processed UPDATE for "
                        << Ipv4Address(prefix) << " from neighbor "
                        << fromNeighborId << ", AS-path length="
                        << localAttrs.AsPathLength()
                        << ", best nexthops=" << entry.bestNextHops.size());
}

void RdmaBgpRouting::WithdrawRoutesFromNeighbor(uint32_t neighborNodeId) {
  std::vector<uint32_t> affectedPrefixes;

  for (auto &[pfx, entry] : m_rib) {
    bool changed = false;
    auto it = entry.paths.begin();
    while (it != entry.paths.end()) {
      bool fromThisNeighbor = false;
      for (auto &nbr : m_neighbors) {
        if (nbr.nodeId == neighborNodeId &&
            nbr.interfaceIdx == it->nextHopInterface) {
          fromThisNeighbor = true;
          break;
        }
      }
      if (fromThisNeighbor) {
        it = entry.paths.erase(it);
        changed = true;
      } else {
        ++it;
      }
    }
    if (changed) {
      entry.SelectBestPaths();
      affectedPrefixes.push_back(pfx);
    }
  }

  for (auto pfx : affectedPrefixes) {
    auto &entry = m_rib[pfx];
    UpdateForwardingTable(pfx, entry);
    if (m_isSwitch && !entry.bestNextHops.empty()) {
      BgpPathAttributes bestForAdv;
      for (auto &p : entry.paths) {
        if (p.nextHopInterface == entry.bestNextHops[0]) {
          bestForAdv = p;
          break;
        }
      }
      AdvertiseToNeighbors(pfx, bestForAdv);
    }
  }

  NS_LOG_DEBUG("Node " << m_nodeId << " withdrew routes from neighbor "
                        << neighborNodeId << ", affected "
                        << affectedPrefixes.size() << " prefixes");
}

void RdmaBgpRouting::AdvertiseToNeighbors(uint32_t prefix,
                                          const BgpPathAttributes &best) {
  for (auto &nbr : m_neighbors) {
    if (best.ContainsAs(nbr.nodeId))
      continue;

    auto *peer = GetPeerMap().count(nbr.nodeId) ? GetPeerMap()[nbr.nodeId]
                                                 : nullptr;
    if (!peer)
      continue;

    BgpPathAttributes adv = best;
    adv.nextHopInterface = -1;
    Simulator::Schedule(m_propagationDelay,
                        &RdmaBgpRouting::ReceiveUpdate, peer, m_nodeId,
                        prefix, adv);
  }
}

void RdmaBgpRouting::UpdateForwardingTable(uint32_t prefix,
                                           BgpRibEntry &entry) {
  if (m_installRouteCb) {
    m_installRouteCb(Ipv4Address(prefix), entry.bestNextHops);
  }
}

} // namespace ns3
