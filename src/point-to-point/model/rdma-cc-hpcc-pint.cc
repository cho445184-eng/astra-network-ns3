#include "rdma-cc-hpcc-pint.h"
#include "rdma-hw.h"
#include "pint.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/data-rate.h"
#include "ns3/log.h"
#include "ns3/int-header.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("RdmaCcHpccPint");
NS_OBJECT_ENSURE_REGISTERED(RdmaCcHpccPint);

TypeId RdmaCcHpccPint::GetTypeId() {
  static TypeId tid = TypeId("ns3::RdmaCcHpccPint")
    .SetParent<RdmaCongestionOps>()
    .SetGroupName("PointToPoint")
    .AddConstructor<RdmaCcHpccPint>()
    .AddAttribute("TargetUtil", "Target utilization",
        DoubleValue(0.95), MakeDoubleAccessor(&RdmaCcHpccPint::m_targetUtil), MakeDoubleChecker<double>())
    .AddAttribute("MiThresh", "Threshold of consecutive AI before MI",
        UintegerValue(5), MakeUintegerAccessor(&RdmaCcHpccPint::m_miThresh), MakeUintegerChecker<uint32_t>())
    .AddAttribute("SmplThresh", "Sampling threshold (out of 65536)",
        UintegerValue(65536), MakeUintegerAccessor(&RdmaCcHpccPint::m_smplThresh), MakeUintegerChecker<uint32_t>())
    .AddAttribute("RateAI", "Additive increase rate",
        DataRateValue(DataRate("5Mb/s")), MakeDataRateAccessor(&RdmaCcHpccPint::m_rai), MakeDataRateChecker())
    .AddAttribute("MinRate", "Minimum sending rate",
        DataRateValue(DataRate("100Mb/s")), MakeDataRateAccessor(&RdmaCcHpccPint::m_minRate), MakeDataRateChecker());
  return tid;
}

RdmaCcHpccPint::RdmaCcHpccPint() {}

void RdmaCcHpccPint::SetSampleThreshold(double p) {
  m_smplThresh = (uint32_t)(65536 * p);
}

void RdmaCcHpccPint::InitQp(Ptr<RdmaQueuePair> qp, DataRate linkRate) {
  qp->hpccPint.m_curRate = linkRate;
}

void RdmaCcHpccPint::HandleAck(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch) {
  uint32_t ack_seq = ch.ack.seq;
  if (rand() % 65536 >= m_smplThresh)
    return;
  if (ack_seq > qp->hpccPint.m_lastUpdateSeq) {
    UpdateRate(qp, p, ch, false);
  } else {
    UpdateRate(qp, p, ch, true);
  }
}

void RdmaCcHpccPint::HandleCnp(Ptr<RdmaQueuePair> qp) {}

void RdmaCcHpccPint::OnQpComplete(Ptr<RdmaQueuePair> qp) {}

Ptr<RdmaCongestionOps> RdmaCcHpccPint::Fork() {
  return CopyObject<RdmaCcHpccPint>(this);
}

void RdmaCcHpccPint::UpdateRate(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch, bool fast_react) {
  uint32_t next_seq = qp->snd_nxt;
  if (qp->hpccPint.m_lastUpdateSeq == 0) {
    qp->hpccPint.m_lastUpdateSeq = next_seq;
  } else {
    IntHeader &ih = ch.ack.ih;
    double U = Pint::decode_u(ih.GetPower());
    DataRate new_rate;
    int32_t new_incStage;
    double max_c = U / m_targetUtil;

    if (max_c >= 1 || qp->hpccPint.m_incStage >= m_miThresh) {
      new_rate = qp->hpccPint.m_curRate / max_c + m_rai;
      new_incStage = 0;
    } else {
      new_rate = qp->hpccPint.m_curRate + m_rai;
      new_incStage = qp->hpccPint.m_incStage + 1;
    }
    if (new_rate < m_minRate) new_rate = m_minRate;
    if (new_rate > qp->m_max_rate) new_rate = qp->m_max_rate;
    m_rdmaHw->ChangeRate(qp, new_rate);
    if (!fast_react) {
      qp->hpccPint.m_curRate = new_rate;
      qp->hpccPint.m_incStage = new_incStage;
    }
    if (!fast_react) {
      if (next_seq > qp->hpccPint.m_lastUpdateSeq)
        qp->hpccPint.m_lastUpdateSeq = next_seq;
    }
  }
}

} // namespace ns3
