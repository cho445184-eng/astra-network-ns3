#include "rdma-cc-timely.h"
#include "rdma-hw.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/data-rate.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("RdmaCcTimely");
NS_OBJECT_ENSURE_REGISTERED(RdmaCcTimely);

TypeId RdmaCcTimely::GetTypeId() {
  static TypeId tid = TypeId("ns3::RdmaCcTimely")
    .SetParent<RdmaCongestionOps>()
    .SetGroupName("PointToPoint")
    .AddConstructor<RdmaCcTimely>()
    .AddAttribute("Alpha", "EWMA weight for RTT gradient",
        DoubleValue(0.875), MakeDoubleAccessor(&RdmaCcTimely::m_alpha), MakeDoubleChecker<double>())
    .AddAttribute("Beta", "Multiplicative decrease factor",
        DoubleValue(0.8), MakeDoubleAccessor(&RdmaCcTimely::m_beta), MakeDoubleChecker<double>())
    .AddAttribute("TLow", "Low RTT threshold (ns)",
        UintegerValue(50000), MakeUintegerAccessor(&RdmaCcTimely::m_tLow), MakeUintegerChecker<uint64_t>())
    .AddAttribute("THigh", "High RTT threshold (ns)",
        UintegerValue(500000), MakeUintegerAccessor(&RdmaCcTimely::m_tHigh), MakeUintegerChecker<uint64_t>())
    .AddAttribute("MinRtt", "Minimum RTT baseline (ns)",
        UintegerValue(20000), MakeUintegerAccessor(&RdmaCcTimely::m_minRtt), MakeUintegerChecker<uint64_t>())
    .AddAttribute("RateAI", "Additive increase rate",
        DataRateValue(DataRate("5Mb/s")), MakeDataRateAccessor(&RdmaCcTimely::m_rai), MakeDataRateChecker())
    .AddAttribute("RateHAI", "Hyper-additive increase rate",
        DataRateValue(DataRate("50Mb/s")), MakeDataRateAccessor(&RdmaCcTimely::m_rhai), MakeDataRateChecker())
    .AddAttribute("MinRate", "Minimum sending rate",
        DataRateValue(DataRate("100Mb/s")), MakeDataRateAccessor(&RdmaCcTimely::m_minRate), MakeDataRateChecker());
  return tid;
}

RdmaCcTimely::RdmaCcTimely() {}

void RdmaCcTimely::InitQp(Ptr<RdmaQueuePair> qp, DataRate linkRate) {
  qp->tmly.m_curRate = linkRate;
}

void RdmaCcTimely::HandleAck(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch) {
  uint32_t ack_seq = ch.ack.seq;
  if (ack_seq > qp->tmly.m_lastUpdateSeq) {
    UpdateRate(qp, p, ch, false);
  }
  // TIMELY fast react is a no-op in the original implementation
}

void RdmaCcTimely::HandleCnp(Ptr<RdmaQueuePair> qp) {
  // TIMELY does not use CNP; it is RTT-based
}

void RdmaCcTimely::OnQpComplete(Ptr<RdmaQueuePair> qp) {}

Ptr<RdmaCongestionOps> RdmaCcTimely::Fork() {
  return CopyObject<RdmaCcTimely>(this);
}

void RdmaCcTimely::UpdateRate(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch, bool us) {
  uint32_t next_seq = qp->snd_nxt;
  uint64_t rtt = Simulator::Now().GetTimeStep() - ch.ack.ih.ts;

  if (qp->tmly.m_lastUpdateSeq != 0) {
    int64_t new_rtt_diff = (int64_t)rtt - (int64_t)qp->tmly.lastRtt;
    double rtt_diff = (1 - m_alpha) * qp->tmly.rttDiff + m_alpha * new_rtt_diff;
    double gradient = rtt_diff / m_minRtt;
    bool inc = false;
    double c = 0;

    if (rtt < m_tLow) {
      inc = true;
    } else if (rtt > m_tHigh) {
      c = 1 - m_beta * (1 - (double)m_tHigh / rtt);
      inc = false;
    } else if (gradient <= 0) {
      inc = true;
    } else {
      c = 1 - m_beta * gradient;
      if (c < 0) c = 0;
      inc = false;
    }

    if (inc) {
      if (qp->tmly.m_incStage < 5) {
        qp->m_rate = qp->tmly.m_curRate + m_rai;
      } else {
        qp->m_rate = qp->tmly.m_curRate + m_rhai;
      }
      if (qp->m_rate > qp->m_max_rate) qp->m_rate = qp->m_max_rate;
      if (!us) {
        qp->tmly.m_curRate = qp->m_rate;
        qp->tmly.m_incStage++;
        qp->tmly.rttDiff = rtt_diff;
      }
    } else {
      qp->m_rate = std::max(m_minRate, qp->tmly.m_curRate * c);
      if (!us) {
        qp->tmly.m_curRate = qp->m_rate;
        qp->tmly.m_incStage = 0;
        qp->tmly.rttDiff = rtt_diff;
      }
    }
  }

  if (!us && next_seq > qp->tmly.m_lastUpdateSeq) {
    qp->tmly.m_lastUpdateSeq = next_seq;
    qp->tmly.lastRtt = rtt;
  }
}

} // namespace ns3
