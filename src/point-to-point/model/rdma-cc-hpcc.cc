#include "rdma-cc-hpcc.h"
#include "rdma-hw.h"
#include "qbb-net-device.h"
#include "ns3/simulator.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/data-rate.h"
#include "ns3/log.h"
#include "ns3/int-header.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("RdmaCcHpcc");
NS_OBJECT_ENSURE_REGISTERED(RdmaCcHpcc);

TypeId RdmaCcHpcc::GetTypeId() {
  static TypeId tid = TypeId("ns3::RdmaCcHpcc")
    .SetParent<RdmaCongestionOps>()
    .SetGroupName("PointToPoint")
    .AddConstructor<RdmaCcHpcc>()
    .AddAttribute("TargetUtil",
        "Target link utilization (default 95%)",
        DoubleValue(0.95),
        MakeDoubleAccessor(&RdmaCcHpcc::m_targetUtil),
        MakeDoubleChecker<double>())
    .AddAttribute("UtilHigh",
        "Upper bound of target utilization (default 98%)",
        DoubleValue(0.98),
        MakeDoubleAccessor(&RdmaCcHpcc::m_utilHigh),
        MakeDoubleChecker<double>())
    .AddAttribute("MiThresh",
        "Threshold of consecutive AI before MI",
        UintegerValue(5),
        MakeUintegerAccessor(&RdmaCcHpcc::m_miThresh),
        MakeUintegerChecker<uint32_t>())
    .AddAttribute("MultiRate",
        "Maintain per-hop rates",
        BooleanValue(true),
        MakeBooleanAccessor(&RdmaCcHpcc::m_multipleRate),
        MakeBooleanChecker())
    .AddAttribute("SampleFeedback",
        "Only react to feedback with qlen > 0 on fast react",
        BooleanValue(false),
        MakeBooleanAccessor(&RdmaCcHpcc::m_sampleFeedback),
        MakeBooleanChecker())
    .AddAttribute("FastReact",
        "Enable fast reaction to congestion feedback",
        BooleanValue(true),
        MakeBooleanAccessor(&RdmaCcHpcc::m_fastReact),
        MakeBooleanChecker())
    .AddAttribute("RateAI",
        "Rate additive increase unit",
        DataRateValue(DataRate("5Mb/s")),
        MakeDataRateAccessor(&RdmaCcHpcc::m_rai),
        MakeDataRateChecker())
    .AddAttribute("MinRate",
        "Minimum sending rate",
        DataRateValue(DataRate("100Mb/s")),
        MakeDataRateAccessor(&RdmaCcHpcc::m_minRate),
        MakeDataRateChecker());
  return tid;
}

RdmaCcHpcc::RdmaCcHpcc() {}

void RdmaCcHpcc::InitQp(Ptr<RdmaQueuePair> qp, DataRate linkRate) {
  qp->hp.m_curRate = linkRate;
  if (m_multipleRate) {
    for (uint32_t i = 0; i < IntHeader::maxHop; i++)
      qp->hp.hopState[i].Rc = linkRate;
  }
}

void RdmaCcHpcc::HandleAck(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch) {
  uint32_t ack_seq = ch.ack.seq;
  if (ack_seq > qp->hp.m_lastUpdateSeq) {
    UpdateRate(qp, p, ch, false);
  } else {
    FastReact(qp, p, ch);
  }
}

void RdmaCcHpcc::HandleCnp(Ptr<RdmaQueuePair> qp) {
  // HPCC does not use explicit CNP; congestion is signaled via INT
}

void RdmaCcHpcc::OnQpComplete(Ptr<RdmaQueuePair> qp) {
  // No timers to cancel for HPCC
}

Ptr<RdmaCongestionOps> RdmaCcHpcc::Fork() {
  return CopyObject<RdmaCcHpcc>(this);
}

void RdmaCcHpcc::FastReact(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch) {
  if (m_fastReact)
    UpdateRate(qp, p, ch, true);
}

void RdmaCcHpcc::UpdateRate(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch, bool fast_react) {
  uint32_t next_seq = qp->snd_nxt;
  if (qp->hp.m_lastUpdateSeq == 0) {
    qp->hp.m_lastUpdateSeq = next_seq;
    IntHeader &ih = ch.ack.ih;
    NS_ASSERT(ih.nhop <= IntHeader::maxHop);
    for (uint32_t i = 0; i < ih.nhop; i++)
      qp->hp.hop[i] = ih.hop[i];
  } else {
    IntHeader &ih = ch.ack.ih;
    if (ih.nhop <= IntHeader::maxHop) {
      double max_c = 0;
      double U = 0;
      uint64_t dt = 0;
      bool updated[IntHeader::maxHop] = {false}, updated_any = false;
      NS_ASSERT(ih.nhop <= IntHeader::maxHop);

      for (uint32_t i = 0; i < ih.nhop; i++) {
        if (m_sampleFeedback) {
          if (ih.hop[i].GetQlen() == 0 && fast_react)
            continue;
        }
        updated[i] = updated_any = true;
        uint64_t tau = ih.hop[i].GetTimeDelta(qp->hp.hop[i]);
        double duration = tau * 1e-9;
        double txRate = (ih.hop[i].GetBytesDelta(qp->hp.hop[i])) * 8 / duration;
        double u = txRate / ih.hop[i].GetLineRate()
          + (double)std::min(ih.hop[i].GetQlen(), qp->hp.hop[i].GetQlen())
            * qp->m_max_rate.GetBitRate() / ih.hop[i].GetLineRate() / qp->m_win;

        if (!m_multipleRate) {
          if (u > U) { U = u; dt = tau; }
        } else {
          if (tau > qp->m_baseRtt) tau = qp->m_baseRtt;
          qp->hp.hopState[i].u = (qp->hp.hopState[i].u * (qp->m_baseRtt - tau) + u * tau) / double(qp->m_baseRtt);
        }
        qp->hp.hop[i] = ih.hop[i];
      }

      DataRate new_rate;
      int32_t new_incStage;
      DataRate new_rate_per_hop[IntHeader::maxHop];
      int32_t new_incStage_per_hop[IntHeader::maxHop];

      if (!m_multipleRate) {
        if (updated_any) {
          if (dt > qp->m_baseRtt) dt = qp->m_baseRtt;
          qp->hp.u = (qp->hp.u * (qp->m_baseRtt - dt) + U * dt) / double(qp->m_baseRtt);
          max_c = qp->hp.u / m_targetUtil;
          if (max_c >= 1 || qp->hp.m_incStage >= m_miThresh) {
            new_rate = qp->hp.m_curRate / max_c + m_rai;
            new_incStage = 0;
          } else {
            new_rate = qp->hp.m_curRate + m_rai;
            new_incStage = qp->hp.m_incStage + 1;
          }
          if (new_rate < m_minRate) new_rate = m_minRate;
          if (new_rate > qp->m_max_rate) new_rate = qp->m_max_rate;
        }
      } else {
        new_rate = qp->m_max_rate;
        for (uint32_t i = 0; i < ih.nhop; i++) {
          if (updated[i]) {
            double c = qp->hp.hopState[i].u / m_targetUtil;
            if (c >= 1 || qp->hp.hopState[i].incStage >= m_miThresh) {
              new_rate_per_hop[i] = qp->hp.hopState[i].Rc / c + m_rai;
              new_incStage_per_hop[i] = 0;
            } else {
              new_rate_per_hop[i] = qp->hp.hopState[i].Rc + m_rai;
              new_incStage_per_hop[i] = qp->hp.hopState[i].incStage + 1;
            }
            if (new_rate_per_hop[i] < m_minRate) new_rate_per_hop[i] = m_minRate;
            if (new_rate_per_hop[i] > qp->m_max_rate) new_rate_per_hop[i] = qp->m_max_rate;
            if (new_rate_per_hop[i] < new_rate) new_rate = new_rate_per_hop[i];
          } else {
            if (qp->hp.hopState[i].Rc < new_rate) new_rate = qp->hp.hopState[i].Rc;
          }
        }
      }

      if (updated_any)
        m_rdmaHw->ChangeRate(qp, new_rate);
      if (!fast_react) {
        if (updated_any) {
          qp->hp.m_curRate = new_rate;
          qp->hp.m_incStage = new_incStage;
        }
        if (m_multipleRate) {
          for (uint32_t i = 0; i < ih.nhop; i++) {
            if (updated[i]) {
              qp->hp.hopState[i].Rc = new_rate_per_hop[i];
              qp->hp.hopState[i].incStage = new_incStage_per_hop[i];
            }
          }
        }
      }
    }
    if (!fast_react) {
      if (next_seq > qp->hp.m_lastUpdateSeq)
        qp->hp.m_lastUpdateSeq = next_seq;
    }
  }
}

} // namespace ns3
