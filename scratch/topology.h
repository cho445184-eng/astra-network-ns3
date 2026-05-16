#ifndef TOPOLOGY_H
#define TOPOLOGY_H

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/qbb-helper.h"
#include <ns3/rdma-bgp-routing.h>
#include <ns3/rdma-driver.h>
#include <ns3/switch-node.h>
#include <map>
#include <vector>

using namespace ns3;
using namespace std;

inline Ipv4Address node_id_to_ip(uint32_t id) {
  return Ipv4Address(0x0b000001 + ((id / 256) * 0x00010000) +
                     ((id % 256) * 0x00000100));
}

inline uint32_t ip_to_node_id(Ipv4Address ip) {
  return (ip.Get() >> 8) & 0xffff;
}

struct Interface {
  uint32_t idx;
  bool up;
  uint64_t delay;
  uint64_t bw;

  Interface() : idx(0), up(false) {}
};

struct TopologyState {
  map<Ptr<Node>, map<Ptr<Node>, Interface>> nbr2if;
  map<Ptr<Node>, map<Ptr<Node>, vector<Ptr<Node>>>> nextHop;
  map<Ptr<Node>, map<Ptr<Node>, uint64_t>> pairDelay;
  map<Ptr<Node>, map<Ptr<Node>, uint64_t>> pairTxDelay;
  map<uint32_t, map<uint32_t, uint64_t>> pairBw;
  map<Ptr<Node>, map<Ptr<Node>, uint64_t>> pairBdp;
  map<uint32_t, map<uint32_t, uint64_t>> pairRtt;
  uint64_t maxRtt = 0, maxBdp = 0;
  uint32_t packet_payload_size = 1000;
};

inline void CalculateRoute(TopologyState &topo, Ptr<Node> host) {
  vector<Ptr<Node>> q;
  map<Ptr<Node>, int> dis;
  map<Ptr<Node>, uint64_t> delay;
  map<Ptr<Node>, uint64_t> txDelay;
  map<Ptr<Node>, uint64_t> bw;
  q.push_back(host);
  dis[host] = 0;
  delay[host] = 0;
  txDelay[host] = 0;
  bw[host] = 0xfffffffffffffffflu;
  for (int i = 0; i < (int)q.size(); i++) {
    Ptr<Node> now = q[i];
    int d = dis[now];
    for (auto it = topo.nbr2if[now].begin(); it != topo.nbr2if[now].end();
         it++) {
      if (!it->second.up)
        continue;
      Ptr<Node> next = it->first;
      if (dis.find(next) == dis.end()) {
        dis[next] = d + 1;
        delay[next] = delay[now] + it->second.delay;
        txDelay[next] =
            txDelay[now] +
            topo.packet_payload_size * 1000000000lu * 8 / it->second.bw;
        bw[next] = std::min(bw[now], it->second.bw);
        if (next->GetNodeType() == 1)
          q.push_back(next);
      }
      if (d + 1 == dis[next]) {
        topo.nextHop[next][host].push_back(now);
      }
    }
  }
  for (auto it : delay)
    topo.pairDelay[it.first][host] = it.second;
  for (auto it : txDelay)
    topo.pairTxDelay[it.first][host] = it.second;
  for (auto it : bw)
    topo.pairBw[it.first->GetId()][host->GetId()] = it.second;
}

inline void CalculateRoutes(TopologyState &topo, NodeContainer &n) {
  for (int i = 0; i < (int)n.GetN(); i++) {
    Ptr<Node> node = n.Get(i);
    if (node->GetNodeType() == 0)
      CalculateRoute(topo, node);
  }
}

inline void SetRoutingEntries(TopologyState &topo) {
  for (auto i = topo.nextHop.begin(); i != topo.nextHop.end(); i++) {
    Ptr<Node> node = i->first;
    auto &table = i->second;
    for (auto j = table.begin(); j != table.end(); j++) {
      Ptr<Node> dst = j->first;
      Ipv4Address dstAddr = dst->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
      vector<Ptr<Node>> nexts = j->second;
      for (int k = 0; k < (int)nexts.size(); k++) {
        Ptr<Node> next = nexts[k];
        uint32_t interface = topo.nbr2if[node][next].idx;
        if (node->GetNodeType() == 1)
          DynamicCast<SwitchNode>(node)->AddTableEntry(dstAddr, interface);
        else {
          node->GetObject<RdmaDriver>()->m_rdma->AddTableEntry(dstAddr,
                                                               interface);
        }
      }
    }
  }
}

inline void TakeDownLink(TopologyState *topo, NodeContainer n, Ptr<Node> a,
                         Ptr<Node> b) {
  if (!topo->nbr2if[a][b].up)
    return;
  topo->nbr2if[a][b].up = topo->nbr2if[b][a].up = false;
  topo->nextHop.clear();
  CalculateRoutes(*topo, n);
  for (uint32_t i = 0; i < n.GetN(); i++) {
    if (n.Get(i)->GetNodeType() == 1)
      DynamicCast<SwitchNode>(n.Get(i))->ClearTable();
    else
      n.Get(i)->GetObject<RdmaDriver>()->m_rdma->ClearTable();
  }
  DynamicCast<QbbNetDevice>(a->GetDevice(topo->nbr2if[a][b].idx))->TakeDown();
  DynamicCast<QbbNetDevice>(b->GetDevice(topo->nbr2if[b][a].idx))->TakeDown();
  SetRoutingEntries(*topo);

  for (uint32_t i = 0; i < n.GetN(); i++) {
    if (n.Get(i)->GetNodeType() == 0)
      n.Get(i)->GetObject<RdmaDriver>()->m_rdma->RedistributeQp();
  }
}

inline void ComputeBdpAndRtt(TopologyState &topo, NodeContainer &n,
                             uint32_t node_num) {
  topo.maxRtt = topo.maxBdp = 0;
  for (uint32_t i = 0; i < node_num; i++) {
    if (n.Get(i)->GetNodeType() != 0)
      continue;
    for (uint32_t j = 0; j < node_num; j++) {
      if (n.Get(j)->GetNodeType() != 0)
        continue;
      uint64_t delay = topo.pairDelay[n.Get(i)][n.Get(j)];
      uint64_t txDelay = topo.pairTxDelay[n.Get(i)][n.Get(j)];
      uint64_t rtt = delay * 2 + txDelay;
      uint64_t bw_val = topo.pairBw[i][j];
      uint64_t bdp = rtt * bw_val / 1000000000 / 8;
      topo.pairBdp[n.Get(i)][n.Get(j)] = bdp;
      topo.pairRtt[i][j] = rtt;
      if (bdp > topo.maxBdp)
        topo.maxBdp = bdp;
      if (rtt > topo.maxRtt)
        topo.maxRtt = rtt;
    }
  }
  printf("maxRtt=%lu maxBdp=%lu\n", topo.maxRtt, topo.maxBdp);
}

inline uint64_t GetNicRate(NodeContainer &n) {
  for (uint32_t i = 0; i < n.GetN(); i++)
    if (n.Get(i)->GetNodeType() == 0)
      return DynamicCast<QbbNetDevice>(n.Get(i)->GetDevice(1))
          ->GetDataRate()
          .GetBitRate();
  return 0;
}

// --- BGP routing ---

struct BgpRoutingState {
  std::map<uint32_t, Ptr<RdmaBgpRouting>> bgpSpeakers;
};

inline void SetupBgpRouting(TopologyState &topo, BgpRoutingState &bgpState,
                            NodeContainer &n, uint32_t defaultLocalPref,
                            double propagationDelayUs) {
  Time propDelay = MicroSeconds(propagationDelayUs);

  for (uint32_t i = 0; i < n.GetN(); i++) {
    Ptr<RdmaBgpRouting> bgp = CreateObject<RdmaBgpRouting>();
    bgp->SetNodeId(n.Get(i)->GetId());
    bgp->SetIsSwitch(n.Get(i)->GetNodeType() == 1);
    bgp->SetPropagationDelay(propDelay);
    bgp->SetAttribute("DefaultLocalPref", UintegerValue(defaultLocalPref));

    uint32_t nodeId = n.Get(i)->GetId();
    auto &neighbors = topo.nbr2if[n.Get(i)];
    for (auto &[peer, iface] : neighbors) {
      if (!iface.up)
        continue;
      bgp->AddNeighbor(peer->GetId(), iface.idx, iface.delay);
    }

    auto installCb = [nodePtr = n.Get(i)](
                         Ipv4Address prefix,
                         const std::vector<int> &nextHops) {
      if (nodePtr->GetNodeType() == 1) {
        Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(nodePtr);
        uint32_t dip = prefix.Get();
        sw->ClearTable();
        auto ribPtr =
            nodePtr->GetObject<RdmaBgpRouting>();
        if (ribPtr) {
          for (auto &[pfx, entry] : ribPtr->GetRib()) {
            Ipv4Address addr(pfx);
            for (int nh : entry.bestNextHops)
              sw->AddTableEntry(addr, nh);
          }
        }
      } else {
        Ptr<RdmaDriver> rdma = nodePtr->GetObject<RdmaDriver>();
        if (rdma && rdma->m_rdma) {
          rdma->m_rdma->ClearTable();
          auto ribPtr =
              nodePtr->GetObject<RdmaBgpRouting>();
          if (ribPtr) {
            for (auto &[pfx, entry] : ribPtr->GetRib()) {
              Ipv4Address addr(pfx);
              for (int nh : entry.bestNextHops)
                rdma->m_rdma->AddTableEntry(addr, nh);
            }
          }
        }
      }
    };
    bgp->SetInstallRouteCallback(installCb);

    n.Get(i)->AggregateObject(bgp);
    bgpState.bgpSpeakers[nodeId] = bgp;
  }

  for (uint32_t i = 0; i < n.GetN(); i++) {
    if (n.Get(i)->GetNodeType() == 0) {
      Ipv4Address hostIp = node_id_to_ip(n.Get(i)->GetId());
      bgpState.bgpSpeakers[n.Get(i)->GetId()]->OriginateRoute(hostIp);
    }
  }
}

inline void TakeDownLinkBgp(TopologyState *topo, BgpRoutingState *bgpState,
                            NodeContainer n, Ptr<Node> a, Ptr<Node> b) {
  if (!topo->nbr2if[a][b].up)
    return;

  topo->nbr2if[a][b].up = topo->nbr2if[b][a].up = false;
  DynamicCast<QbbNetDevice>(a->GetDevice(topo->nbr2if[a][b].idx))->TakeDown();
  DynamicCast<QbbNetDevice>(b->GetDevice(topo->nbr2if[b][a].idx))->TakeDown();

  auto *bgpA = bgpState->bgpSpeakers.count(a->GetId())
                   ? GetPointer(bgpState->bgpSpeakers[a->GetId()])
                   : nullptr;
  auto *bgpB = bgpState->bgpSpeakers.count(b->GetId())
                   ? GetPointer(bgpState->bgpSpeakers[b->GetId()])
                   : nullptr;
  if (bgpA)
    bgpA->WithdrawRoutesFromNeighbor(b->GetId());
  if (bgpB)
    bgpB->WithdrawRoutesFromNeighbor(a->GetId());

  for (uint32_t i = 0; i < n.GetN(); i++) {
    if (n.Get(i)->GetNodeType() == 0)
      n.Get(i)->GetObject<RdmaDriver>()->m_rdma->RedistributeQp();
  }
}

#endif // TOPOLOGY_H

