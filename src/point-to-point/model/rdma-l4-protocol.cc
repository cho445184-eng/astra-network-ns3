#include "rdma-l4-protocol.h"
#include "rdma-hw.h"
#include "ppp-header.h"
#include "ns3/log.h"
#include "ns3/ipv4.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/custom-header.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("RdmaL4Protocol");
NS_OBJECT_ENSURE_REGISTERED(RdmaL4Protocol);

TypeId RdmaL4Protocol::GetTypeId() {
  static TypeId tid = TypeId("ns3::RdmaL4Protocol")
    .SetParent<IpL4Protocol>()
    .SetGroupName("PointToPoint")
    .AddConstructor<RdmaL4Protocol>();
  return tid;
}

RdmaL4Protocol::RdmaL4Protocol() {}
RdmaL4Protocol::~RdmaL4Protocol() {}

void RdmaL4Protocol::SetNode(Ptr<Node> node) {
  m_node = node;
}

void RdmaL4Protocol::SetRdmaHw(Ptr<RdmaHw> hw) {
  m_rdmaHw = hw;
}

int RdmaL4Protocol::GetProtocolNumber() const {
  // Return ACK protocol number as the primary; we register multiple
  // protocol numbers explicitly via Ipv4L3Protocol::Insert
  return PROT_NUMBER_ACK;
}

void RdmaL4Protocol::NotifyNewAggregate() {
  Ptr<Ipv4> ipv4 = GetObject<Ipv4>();
  if (ipv4 && m_downTarget.IsNull()) {
    m_downTarget = MakeCallback(&Ipv4::Send, ipv4);
    // Register for each RDMA protocol number
    Ptr<Ipv4L3Protocol> ipv4l3 = DynamicCast<Ipv4L3Protocol>(ipv4);
    if (ipv4l3) {
      ipv4l3->Insert(this, PROT_NUMBER_ACK);
      ipv4l3->Insert(this, PROT_NUMBER_NACK);
      ipv4l3->Insert(this, PROT_NUMBER_CNP);
    }
  }
  IpL4Protocol::NotifyNewAggregate();
}

IpL4Protocol::RxStatus
RdmaL4Protocol::Receive(Ptr<Packet> p,
                         const Ipv4Header &header,
                         Ptr<Ipv4Interface> incomingInterface) {
  NS_LOG_FUNCTION(this << p << header);

  if (!m_rdmaHw) {
    NS_LOG_WARN("RdmaL4Protocol::Receive called but no RdmaHw set");
    return IpL4Protocol::RX_ENDPOINT_CLOSED;
  }

  // Reconstruct CustomHeader for compatibility with RdmaHw::Receive
  // The packet has already had its L2/L3 headers stripped by Ipv4L3Protocol
  CustomHeader ch(CustomHeader::L2_Header | CustomHeader::L3_Header | CustomHeader::L4_Header);
  ch.getInt = 1;

  // Build a complete packet with headers for CustomHeader parsing
  // (RdmaHw::Receive expects the full header chain to be parseable)
  Ptr<Packet> fullPkt = p->Copy();
  fullPkt->AddHeader(header);
  PppHeader ppp;
  ppp.SetProtocol(0x0021);
  fullPkt->AddHeader(ppp);
  fullPkt->PeekHeader(ch);

  m_rdmaHw->Receive(fullPkt, ch);
  return IpL4Protocol::RX_OK;
}

IpL4Protocol::RxStatus
RdmaL4Protocol::Receive(Ptr<Packet> p,
                         const Ipv6Header &header,
                         Ptr<Ipv6Interface> incomingInterface) {
  // IPv6 not used in RDMA simulation
  return IpL4Protocol::RX_ENDPOINT_CLOSED;
}

void RdmaL4Protocol::SendDown(Ptr<Packet> p, Ipv4Address src,
                                Ipv4Address dst, uint8_t protocol) {
  if (!m_downTarget.IsNull()) {
    m_downTarget(p, src, dst, protocol, nullptr);
  }
}

void RdmaL4Protocol::SetDownTarget(IpL4Protocol::DownTargetCallback cb) {
  m_downTarget = cb;
}

void RdmaL4Protocol::SetDownTarget6(IpL4Protocol::DownTargetCallback6 cb) {
  m_downTarget6 = cb;
}

IpL4Protocol::DownTargetCallback RdmaL4Protocol::GetDownTarget() const {
  return m_downTarget;
}

IpL4Protocol::DownTargetCallback6 RdmaL4Protocol::GetDownTarget6() const {
  return m_downTarget6;
}

} // namespace ns3
