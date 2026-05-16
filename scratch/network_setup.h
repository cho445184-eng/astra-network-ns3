#ifndef NETWORK_SETUP_H
#define NETWORK_SETUP_H

#include "cc_factory.h"
#include "monitoring.h"
#include "sim_config.h"
#include "topology.h"

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/error-model.h"
#include "ns3/global-route-manager.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/packet.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/qbb-helper.h"
#include <ns3/rdma-client-helper.h>
#include <ns3/rdma-client.h>
#include <ns3/rdma-driver.h>
#include <ns3/rdma.h>
#include <ns3/sim-setting.h>
#include <ns3/switch-node.h>

using namespace ns3;
using namespace std;

struct FlowInput {
  uint32_t src, dst, pg, maxPacketCount, port, dport;
  double start_time;
  uint32_t idx;
};

struct NetworkContext {
  NodeContainer nodes;
  TopologyState topology;
  BgpRoutingState bgpState;
  vector<Ipv4Address> serverAddress;
  unordered_map<uint32_t, unordered_map<uint32_t, uint16_t>> portNumber;
  uint64_t nic_rate = 0;
  uint32_t flow_num = 0;
  FlowInput flow_input = {0};
};

inline void get_pfc(FILE *fout, Ptr<QbbNetDevice> dev, uint32_t type) {
  fprintf(fout, "%lu %u %u %u %u\n", Simulator::Now().GetTimeStep(),
          dev->GetNode()->GetId(), dev->GetNode()->GetNodeType(),
          dev->GetIfIndex(), type);
}

inline void SetConfig(const SimConfig &cfg) {
  Config::SetDefault("ns3::QbbNetDevice::PauseTime",
                     UintegerValue(cfg.pause_time));
  Config::SetDefault("ns3::QbbNetDevice::QcnEnabled",
                     BooleanValue(cfg.enable_qcn));
  Config::SetDefault("ns3::QbbNetDevice::DynamicThreshold",
                     BooleanValue(cfg.use_dynamic_pfc_threshold));

  IntHop::multi = cfg.int_multi;
  if (cfg.cc_mode == 7)
    IntHeader::mode = IntHeader::TS;
  else if (cfg.cc_mode == 3)
    IntHeader::mode = IntHeader::NORMAL;
  else if (cfg.cc_mode == 10)
    IntHeader::mode = IntHeader::PINT;
  else
    IntHeader::mode = IntHeader::NONE;

  if (cfg.cc_mode == 10) {
    Pint::set_log_base(cfg.pint_log_base);
    IntHeader::pint_bytes = Pint::get_n_bytes();
    printf("PINT bits: %d bytes: %d\n", Pint::get_n_bits(),
           Pint::get_n_bytes());
  }
}

inline bool SetupNetwork(const SimConfig &cfg, NetworkContext &ctx,
                         void (*qp_finish)(FILE *, Ptr<RdmaQueuePair>)) {

  // --- open input files ---
  std::ifstream topof(cfg.topology_file.c_str());
  if (!topof.is_open()) {
    std::cerr << "Error: cannot open topology file: " << cfg.topology_file
              << std::endl;
    return false;
  }
  std::ifstream flowf(cfg.flow_file.c_str());
  if (!flowf.is_open()) {
    std::cerr << "Error: cannot open flow file: " << cfg.flow_file
              << std::endl;
    return false;
  }
  std::ifstream tracef(cfg.trace_file.c_str());
  if (!tracef.is_open()) {
    std::cerr << "Error: cannot open trace file: " << cfg.trace_file
              << std::endl;
    return false;
  }

  uint32_t node_num, switch_num, link_num, trace_num;
  topof >> node_num >> switch_num >> link_num;
  flowf >> ctx.flow_num;
  tracef >> trace_num;

  // --- create nodes ---
  vector<uint32_t> node_type(node_num, 0);
  for (uint32_t i = 0; i < switch_num; i++) {
    uint32_t sid;
    topof >> sid;
    node_type[sid] = 1;
  }
  for (uint32_t i = 0; i < node_num; i++) {
    if (node_type[i] == 0)
      ctx.nodes.Add(CreateObject<Node>());
    else {
      Ptr<SwitchNode> sw = CreateObject<SwitchNode>();
      ctx.nodes.Add(sw);
      sw->SetAttribute("EcnEnabled", BooleanValue(cfg.enable_qcn));
    }
  }

  NS_LOG_INFO("Create nodes.");
  InternetStackHelper internet;
  internet.Install(ctx.nodes);

  // --- assign server IPs ---
  for (uint32_t i = 0; i < node_num; i++) {
    if (ctx.nodes.Get(i)->GetNodeType() == 0) {
      ctx.serverAddress.resize(i + 1);
      ctx.serverAddress[i] = node_id_to_ip(i);
    }
  }

  NS_LOG_INFO("Create channels.");

  // --- create links ---
  Ptr<RateErrorModel> rem = CreateObject<RateErrorModel>();
  Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
  rem->SetRandomVariable(uv);
  uv->SetStream(50);
  rem->SetAttribute("ErrorRate", DoubleValue(cfg.error_rate_per_link));
  rem->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));

  FILE *pfc_file = fopen(cfg.pfc_output_file.c_str(), "w");

  QbbHelper qbb;
  Ipv4AddressHelper ipv4;
  for (uint32_t i = 0; i < link_num; i++) {
    uint32_t src, dst;
    std::string data_rate, link_delay;
    double error_rate;
    topof >> src >> dst >> data_rate >> link_delay >> error_rate;
    Ptr<Node> snode = ctx.nodes.Get(src), dnode = ctx.nodes.Get(dst);

    qbb.SetDeviceAttribute("DataRate", StringValue(data_rate));
    qbb.SetChannelAttribute("Delay", StringValue(link_delay));

    if (error_rate > 0) {
      Ptr<RateErrorModel> rem2 = CreateObject<RateErrorModel>();
      Ptr<UniformRandomVariable> uv2 = CreateObject<UniformRandomVariable>();
      rem2->SetRandomVariable(uv2);
      uv2->SetStream(50);
      rem2->SetAttribute("ErrorRate", DoubleValue(error_rate));
      rem2->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));
      qbb.SetDeviceAttribute("ReceiveErrorModel", PointerValue(rem2));
    } else {
      qbb.SetDeviceAttribute("ReceiveErrorModel", PointerValue(rem));
    }

    fflush(stdout);

    NetDeviceContainer d = qbb.Install(snode, dnode);
    if (snode->GetNodeType() == 0) {
      Ptr<Ipv4> ipv4Obj = snode->GetObject<Ipv4>();
      ipv4Obj->AddInterface(d.Get(0));
      ipv4Obj->AddAddress(
          1,
          Ipv4InterfaceAddress(ctx.serverAddress[src], Ipv4Mask(0xff000000)));
    }
    if (dnode->GetNodeType() == 0) {
      Ptr<Ipv4> ipv4Obj = dnode->GetObject<Ipv4>();
      ipv4Obj->AddInterface(d.Get(1));
      ipv4Obj->AddAddress(
          1,
          Ipv4InterfaceAddress(ctx.serverAddress[dst], Ipv4Mask(0xff000000)));
    }

    ctx.topology.nbr2if[snode][dnode].idx =
        DynamicCast<QbbNetDevice>(d.Get(0))->GetIfIndex();
    ctx.topology.nbr2if[snode][dnode].up = true;
    ctx.topology.nbr2if[snode][dnode].delay =
        DynamicCast<QbbChannel>(
            DynamicCast<QbbNetDevice>(d.Get(0))->GetChannel())
            ->GetDelay()
            .GetTimeStep();
    ctx.topology.nbr2if[snode][dnode].bw =
        DynamicCast<QbbNetDevice>(d.Get(0))->GetDataRate().GetBitRate();
    ctx.topology.nbr2if[dnode][snode].idx =
        DynamicCast<QbbNetDevice>(d.Get(1))->GetIfIndex();
    ctx.topology.nbr2if[dnode][snode].up = true;
    ctx.topology.nbr2if[dnode][snode].delay =
        DynamicCast<QbbChannel>(
            DynamicCast<QbbNetDevice>(d.Get(1))->GetChannel())
            ->GetDelay()
            .GetTimeStep();
    ctx.topology.nbr2if[dnode][snode].bw =
        DynamicCast<QbbNetDevice>(d.Get(1))->GetDataRate().GetBitRate();

    char ipstring[16];
    sprintf(ipstring, "10.%d.%d.0", i / 254 + 1, i % 254 + 1);
    ipv4.SetBase(ipstring, "255.255.255.0");
    ipv4.Assign(d);

    DynamicCast<QbbNetDevice>(d.Get(0))
        ->TraceConnectWithoutContext(
            "QbbPfc", MakeBoundCallback(&get_pfc, pfc_file,
                                        DynamicCast<QbbNetDevice>(d.Get(0))));
    DynamicCast<QbbNetDevice>(d.Get(1))
        ->TraceConnectWithoutContext(
            "QbbPfc", MakeBoundCallback(&get_pfc, pfc_file,
                                        DynamicCast<QbbNetDevice>(d.Get(1))));
  }

  // --- configure switches ---
  ctx.nic_rate = GetNicRate(ctx.nodes);
  for (uint32_t i = 0; i < node_num; i++) {
    if (ctx.nodes.Get(i)->GetNodeType() == 1) {
      Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(ctx.nodes.Get(i));
      uint32_t shift = 3;

      for (uint32_t j = 1; j < sw->GetNDevices(); j++) {
        Ptr<QbbNetDevice> dev = DynamicCast<QbbNetDevice>(sw->GetDevice(j));
        uint64_t rate = dev->GetDataRate().GetBitRate();
        NS_ASSERT_MSG(cfg.rate2kmin.find(rate) != cfg.rate2kmin.end(),
                      "must set kmin for each link speed");
        NS_ASSERT_MSG(cfg.rate2kmax.find(rate) != cfg.rate2kmax.end(),
                      "must set kmax for each link speed");
        NS_ASSERT_MSG(cfg.rate2pmax.find(rate) != cfg.rate2pmax.end(),
                      "must set pmax for each link speed");
        sw->m_mmu->ConfigEcn(j, cfg.rate2kmin.at(rate),
                             cfg.rate2kmax.at(rate), cfg.rate2pmax.at(rate));
        uint64_t delay = DynamicCast<QbbChannel>(dev->GetChannel())
                             ->GetDelay()
                             .GetTimeStep();
        uint32_t headroom = rate * delay / 8 / 1000000000 * cfg.headroom_factor;
        sw->m_mmu->ConfigHdrm(j, headroom);

        sw->m_mmu->pfc_a_shift[j] = shift;
        while (rate > ctx.nic_rate && sw->m_mmu->pfc_a_shift[j] > 0) {
          sw->m_mmu->pfc_a_shift[j]--;
          rate /= 2;
        }
      }
      sw->m_mmu->ConfigNPort(sw->GetNDevices() - 1);
      sw->m_mmu->ConfigBufferSize(cfg.buffer_size * 1024 * 1024);
      sw->m_mmu->node_id = sw->GetId();
    }
  }

  // --- install RDMA drivers ---
#if ENABLE_QP
  FILE *fct_output = fopen(cfg.fct_output_file.c_str(), "w");
  std::cout << "QP is enabled " << std::endl;
  for (uint32_t i = 0; i < node_num; i++) {
    if (ctx.nodes.Get(i)->GetNodeType() == 0) {
      Ptr<RdmaHw> rdmaHw = CreateObject<RdmaHw>();
      rdmaHw->SetAttribute("L2BackToZero", BooleanValue(cfg.l2_back_to_zero));
      rdmaHw->SetAttribute("L2ChunkSize", UintegerValue(cfg.l2_chunk_size));
      rdmaHw->SetAttribute("L2AckInterval", UintegerValue(cfg.l2_ack_interval));
      rdmaHw->SetAttribute("Mtu", UintegerValue(cfg.packet_payload_size));
      rdmaHw->SetAttribute("VarWin", BooleanValue(cfg.var_win));
      rdmaHw->SetAttribute("RateBound", BooleanValue(cfg.rate_bound));
      rdmaHw->SetAttribute("TotalPauseTimes",
                           UintegerValue(cfg.nic_total_pause_time));

      rdmaHw->m_ccOps = CreateCongestionControlOps(cfg);

      Ptr<RdmaDriver> rdma = CreateObject<RdmaDriver>();
      Ptr<Node> node = ctx.nodes.Get(i);
      rdma->SetNode(node);
      rdma->SetRdmaHw(rdmaHw);

      node->AggregateObject(rdma);
      rdma->Init();
      rdma->TraceConnectWithoutContext(
          "QpComplete", MakeBoundCallback(qp_finish, fct_output));
    }
  }
#endif

  // --- ACK priority ---
  if (cfg.ack_high_prio)
    RdmaEgressQueue::ack_q_idx = 0;
  else
    RdmaEgressQueue::ack_q_idx = 3;

  // --- routing ---
  ctx.topology.packet_payload_size = cfg.packet_payload_size;
  if (cfg.routing_protocol == "bgp") {
    // BGP: routes computed via BFS first (for delay/BDP metrics), then
    // BGP distributes routes through UPDATE messages with convergence.
    // BFS provides the topology metrics; BGP provides the actual forwarding.
    CalculateRoutes(ctx.topology, ctx.nodes);
    // ComputeBdpAndRtt needs pairDelay/pairBw which are set by CalculateRoutes
    ComputeBdpAndRtt(ctx.topology, ctx.nodes, node_num);
    // Set up BGP speakers and originate routes.
    // BGP UPDATE messages are scheduled as ns-3 events at microsecond scale,
    // so convergence completes well before any application flows start.
    SetupBgpRouting(ctx.topology, ctx.bgpState, ctx.nodes,
                    cfg.bgp_local_pref, cfg.bgp_propagation_delay_us);
    // Also install static routes as fallback to ensure BDP/RTT metrics work
    SetRoutingEntries(ctx.topology);
  } else {
    // Static BFS routing (default)
    CalculateRoutes(ctx.topology, ctx.nodes);
    SetRoutingEntries(ctx.topology);
    ComputeBdpAndRtt(ctx.topology, ctx.nodes, node_num);
  }

  // --- switch CC ---
  for (uint32_t i = 0; i < node_num; i++) {
    if (ctx.nodes.Get(i)->GetNodeType() == 1) {
      Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(ctx.nodes.Get(i));
      sw->SetAttribute("CcMode", UintegerValue(cfg.cc_mode));
      sw->SetAttribute("MaxRtt", UintegerValue(ctx.topology.maxRtt));
    }
  }

  // --- tracing ---
  NodeContainer trace_nodes;
  for (uint32_t i = 0; i < trace_num; i++) {
    uint32_t nid;
    tracef >> nid;
    if (nid >= ctx.nodes.GetN())
      continue;
    trace_nodes = NodeContainer(trace_nodes, ctx.nodes.Get(nid));
  }

  FILE *trace_output = fopen(cfg.trace_output_file.c_str(), "w");
  if (cfg.enable_trace)
    qbb.EnableTracing(trace_output, trace_nodes);

  {
    SimSetting sim_setting;
    for (auto i : ctx.topology.nbr2if) {
      for (auto j : i.second) {
        uint16_t node = i.first->GetId();
        uint8_t intf = j.second.idx;
        uint64_t bps =
            DynamicCast<QbbNetDevice>(i.first->GetDevice(j.second.idx))
                ->GetDataRate()
                .GetBitRate();
        sim_setting.port_speed[node][intf] = bps;
      }
    }
    sim_setting.win = ctx.topology.maxBdp;
    sim_setting.Serialize(trace_output);
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  NS_LOG_INFO("Create Applications.");

  Time interPacketInterval = Seconds(0.0000005 / 2);
  for (uint32_t i = 0; i < node_num; i++) {
    if (ctx.nodes.Get(i)->GetNodeType() == 0)
      for (uint32_t j = 0; j < node_num; j++) {
        if (ctx.nodes.Get(j)->GetNodeType() == 0)
          ctx.portNumber[i][j] = 10000;
      }
  }
  ctx.flow_input.idx = -1;

  topof.close();
  tracef.close();

  // --- schedule link down ---
  if (cfg.link_down_time > 0) {
    if (cfg.routing_protocol == "bgp") {
      Simulator::Schedule(Seconds(2) + MicroSeconds(cfg.link_down_time),
                          &TakeDownLinkBgp, &ctx.topology, &ctx.bgpState,
                          ctx.nodes, ctx.nodes.Get(cfg.link_down_A),
                          ctx.nodes.Get(cfg.link_down_B));
    } else {
      Simulator::Schedule(Seconds(2) + MicroSeconds(cfg.link_down_time),
                          &TakeDownLink, &ctx.topology, ctx.nodes,
                          ctx.nodes.Get(cfg.link_down_A),
                          ctx.nodes.Get(cfg.link_down_B));
    }
  }

  // --- schedule buffer monitor ---
  FILE *qlen_output = fopen(cfg.qlen_mon_file.c_str(), "w");
  Simulator::Schedule(NanoSeconds(cfg.qlen_mon_start), &monitor_buffer,
                      qlen_output, &ctx.nodes, cfg.qlen_mon_interval);

  return true;
}

#endif // NETWORK_SETUP_H
