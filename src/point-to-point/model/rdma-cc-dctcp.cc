#include "rdma-cc-dctcp.h"
#include "rdma-hw.h"
#include "qbb-header.h"
#include "ns3/simulator.h"
#include "ns3/double.h"
#include "ns3/uinteger.h"
#include "ns3/data-rate.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("RdmaCcDctcp");
NS_OBJECT_ENSURE_REGISTERED(RdmaCcDctcp);

TypeId RdmaCcDctcp::GetTypeId() {
  static TypeId tid = TypeId("ns3::RdmaCcDctcp")
    .SetParent<RdmaCongestionOps>()
    .SetGroupName("PointToPoint")
    .AddConstructor<RdmaCcDctcp>()
    .AddAttribute("EwmaGain", "EWMA gain for alpha update",
        DoubleValue(1.0 / 16), MakeDoubleAccessor(&RdmaCcDctcp::m_g), MakeDoubleChecker<double>())
    .AddAttribute("RateAI", "Additive increase rate",
        DataRateValue(DataRate("1000Mb/s")), MakeDataRateAccessor(&RdmaCcDctcp::m_rai), MakeDataRateChecker())
    .AddAttribute("MinRate", "Minimum sending rate",
        DataRateValue(DataRate("100Mb/s")), MakeDataRateAccessor(&RdmaCcDctcp::m_minRate), MakeDataRateChecker())
    .AddAttribute("Mtu", "MTU for batch size calculation",
        UintegerValue(1000), MakeUintegerAccessor(&RdmaCcDctcp::m_mtu), MakeUintegerChecker<uint32_t>());
  return tid;
}

RdmaCcDctcp::RdmaCcDctcp() {}

void RdmaCcDctcp::InitQp(Ptr<RdmaQueuePair> qp, DataRate linkRate) {
  // DCTCP initializes with link rate (already set by RdmaHw)
}

void RdmaCcDctcp::HandleAck(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch) {
  uint32_t ack_seq = ch.ack.seq;
  uint8_t cnp = (ch.ack.flags >> qbbHeader::FLAG_CNP) & 1;
  bool new_batch = false;

  qp->dctcp.m_ecnCnt += (cnp > 0);
  if (ack_seq > qp->dctcp.m_lastUpdateSeq) {
    new_batch = true;
    if (qp->dctcp.m_lastUpdateSeq == 0) {
      qp->dctcp.m_lastUpdateSeq = qp->snd_nxt;
      qp->dctcp.m_batchSizeOfAlpha = qp->snd_nxt / m_mtu + 1;
    } else {
      double frac = std::min(1.0, double(qp->dctcp.m_ecnCnt) / qp->dctcp.m_batchSizeOfAlpha);
      qp->dctcp.m_alpha = (1 - m_g) * qp->dctcp.m_alpha + m_g * frac;
      qp->dctcp.m_lastUpdateSeq = qp->snd_nxt;
      qp->dctcp.m_ecnCnt = 0;
      qp->dctcp.m_batchSizeOfAlpha = (qp->snd_nxt - ack_seq) / m_mtu + 1;
    }
  }

  if (qp->dctcp.m_caState == 1) {
    if (ack_seq > qp->dctcp.m_highSeq)
      qp->dctcp.m_caState = 0;
  }

  if (cnp && qp->dctcp.m_caState == 0) {
    qp->m_rate = std::max(m_minRate, qp->m_rate * (1 - qp->dctcp.m_alpha / 2));
    qp->dctcp.m_caState = 1;
    qp->dctcp.m_highSeq = qp->snd_nxt;
  }

  if (qp->dctcp.m_caState == 0 && new_batch)
    qp->m_rate = std::min(qp->m_max_rate, qp->m_rate + m_rai);
}

void RdmaCcDctcp::HandleCnp(Ptr<RdmaQueuePair> qp) {
  // DCTCP uses ECN-marked ACKs, not standalone CNP
}

void RdmaCcDctcp::OnQpComplete(Ptr<RdmaQueuePair> qp) {}

Ptr<RdmaCongestionOps> RdmaCcDctcp::Fork() {
  return CopyObject<RdmaCcDctcp>(this);
}

} // namespace ns3
