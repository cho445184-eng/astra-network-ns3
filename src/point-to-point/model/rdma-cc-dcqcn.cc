#include "rdma-cc-dcqcn.h"
#include "rdma-hw.h"
#include "qbb-net-device.h"
#include "ns3/simulator.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/data-rate.h"
#include "ns3/log.h"
#include "qbb-header.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("RdmaCcDcqcn");
NS_OBJECT_ENSURE_REGISTERED(RdmaCcDcqcn);

TypeId RdmaCcDcqcn::GetTypeId() {
  static TypeId tid = TypeId("ns3::RdmaCcDcqcn")
    .SetParent<RdmaCongestionOps>()
    .SetGroupName("PointToPoint")
    .AddConstructor<RdmaCcDcqcn>()
    .AddAttribute("EwmaGain",
        "Control gain parameter which determines the level of rate decrease",
        DoubleValue(1.0 / 16),
        MakeDoubleAccessor(&RdmaCcDcqcn::m_g),
        MakeDoubleChecker<double>())
    .AddAttribute("RateOnFirstCnp",
        "The fraction of rate on first CNP",
        DoubleValue(1.0),
        MakeDoubleAccessor(&RdmaCcDcqcn::m_rateOnFirstCNP),
        MakeDoubleChecker<double>())
    .AddAttribute("ClampTargetRate",
        "Clamp target rate on rate decrease",
        BooleanValue(false),
        MakeBooleanAccessor(&RdmaCcDcqcn::m_ecnClampTgtRate),
        MakeBooleanChecker())
    .AddAttribute("RPTimer",
        "The rate increase timer at RP in microseconds",
        DoubleValue(1500.0),
        MakeDoubleAccessor(&RdmaCcDcqcn::m_rpgTimeReset),
        MakeDoubleChecker<double>())
    .AddAttribute("RateDecreaseInterval",
        "The interval of rate decrease check in microseconds",
        DoubleValue(4.0),
        MakeDoubleAccessor(&RdmaCcDcqcn::m_rateDecreaseInterval),
        MakeDoubleChecker<double>())
    .AddAttribute("FastRecoveryTimes",
        "The threshold between fast recovery and active increase",
        UintegerValue(5),
        MakeUintegerAccessor(&RdmaCcDcqcn::m_rpgThreshold),
        MakeUintegerChecker<uint32_t>())
    .AddAttribute("AlphaResumeInterval",
        "The interval of resuming alpha in microseconds",
        DoubleValue(55.0),
        MakeDoubleAccessor(&RdmaCcDcqcn::m_alphaResumeInterval),
        MakeDoubleChecker<double>())
    .AddAttribute("RateAI",
        "Rate increment unit in AI period",
        DataRateValue(DataRate("5Mb/s")),
        MakeDataRateAccessor(&RdmaCcDcqcn::m_rai),
        MakeDataRateChecker())
    .AddAttribute("RateHAI",
        "Rate increment unit in hyper-active AI period",
        DataRateValue(DataRate("50Mb/s")),
        MakeDataRateAccessor(&RdmaCcDcqcn::m_rhai),
        MakeDataRateChecker())
    .AddAttribute("MinRate",
        "Minimum rate of a throttled flow",
        DataRateValue(DataRate("100Mb/s")),
        MakeDataRateAccessor(&RdmaCcDcqcn::m_minRate),
        MakeDataRateChecker());
  return tid;
}

RdmaCcDcqcn::RdmaCcDcqcn() {}

void RdmaCcDcqcn::InitQp(Ptr<RdmaQueuePair> qp, DataRate linkRate) {
  qp->mlx.m_targetRate = linkRate;
}

void RdmaCcDcqcn::HandleAck(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch) {
  // DCQCN reacts to CNP flag only (handled via HandleCnp path in RdmaHw)
}

void RdmaCcDcqcn::HandleCnp(Ptr<RdmaQueuePair> q) {
  q->mlx.m_alpha_cnp_arrived = true;
  q->mlx.m_decrease_cnp_arrived = true;
  if (q->mlx.m_first_cnp) {
    q->mlx.m_alpha = 1;
    q->mlx.m_alpha_cnp_arrived = false;
    ScheduleUpdateAlpha(q);
    ScheduleDecreaseRate(q, 1);
    q->mlx.m_targetRate = q->m_rate = m_rateOnFirstCNP * q->m_rate;
    q->mlx.m_first_cnp = false;
  }
}

void RdmaCcDcqcn::OnQpComplete(Ptr<RdmaQueuePair> qp) {
  Simulator::Cancel(qp->mlx.m_eventUpdateAlpha);
  Simulator::Cancel(qp->mlx.m_eventDecreaseRate);
  Simulator::Cancel(qp->mlx.m_rpTimer);
}

Ptr<RdmaCongestionOps> RdmaCcDcqcn::Fork() {
  return CopyObject<RdmaCcDcqcn>(this);
}

void RdmaCcDcqcn::UpdateAlpha(Ptr<RdmaQueuePair> q) {
  if (q->mlx.m_alpha_cnp_arrived) {
    q->mlx.m_alpha = (1 - m_g) * q->mlx.m_alpha + m_g;
  } else {
    q->mlx.m_alpha = (1 - m_g) * q->mlx.m_alpha;
  }
  q->mlx.m_alpha_cnp_arrived = false;
  ScheduleUpdateAlpha(q);
}

void RdmaCcDcqcn::ScheduleUpdateAlpha(Ptr<RdmaQueuePair> q) {
  q->mlx.m_eventUpdateAlpha = Simulator::Schedule(
      MicroSeconds(m_alphaResumeInterval), &RdmaCcDcqcn::UpdateAlpha, this, q);
}

void RdmaCcDcqcn::CheckRateDecrease(Ptr<RdmaQueuePair> q) {
  ScheduleDecreaseRate(q, 0);
  if (q->mlx.m_decrease_cnp_arrived) {
    bool clamp = true;
    if (!m_ecnClampTgtRate) {
      if (q->mlx.m_rpTimeStage == 0)
        clamp = false;
    }
    if (clamp)
      q->mlx.m_targetRate = q->m_rate;
    q->m_rate = std::max(m_minRate, q->m_rate * (1 - q->mlx.m_alpha / 2));
    q->mlx.m_rpTimeStage = 0;
    q->mlx.m_decrease_cnp_arrived = false;
    Simulator::Cancel(q->mlx.m_rpTimer);
    q->mlx.m_rpTimer = Simulator::Schedule(
        MicroSeconds(m_rpgTimeReset), &RdmaCcDcqcn::RateIncEventTimer, this, q);
  }
}

void RdmaCcDcqcn::ScheduleDecreaseRate(Ptr<RdmaQueuePair> q, uint32_t delta) {
  q->mlx.m_eventDecreaseRate = Simulator::Schedule(
      MicroSeconds(m_rateDecreaseInterval) + NanoSeconds(delta),
      &RdmaCcDcqcn::CheckRateDecrease, this, q);
}

void RdmaCcDcqcn::RateIncEventTimer(Ptr<RdmaQueuePair> q) {
  q->mlx.m_rpTimer = Simulator::Schedule(
      MicroSeconds(m_rpgTimeReset), &RdmaCcDcqcn::RateIncEventTimer, this, q);
  RateIncEvent(q);
  q->mlx.m_rpTimeStage++;
}

void RdmaCcDcqcn::RateIncEvent(Ptr<RdmaQueuePair> q) {
  if (q->mlx.m_rpTimeStage < m_rpgThreshold) {
    FastRecovery(q);
  } else if (q->mlx.m_rpTimeStage == m_rpgThreshold) {
    ActiveIncrease(q);
  } else {
    HyperIncrease(q);
  }
}

void RdmaCcDcqcn::FastRecovery(Ptr<RdmaQueuePair> q) {
  q->m_rate = (q->m_rate / 2) + (q->mlx.m_targetRate / 2);
}

void RdmaCcDcqcn::ActiveIncrease(Ptr<RdmaQueuePair> q) {
  uint32_t nic_idx = m_rdmaHw->GetNicIdxOfQp(q);
  Ptr<QbbNetDevice> dev = m_rdmaHw->m_nic[nic_idx].dev;
  q->mlx.m_targetRate += m_rai;
  if (q->mlx.m_targetRate > dev->GetDataRate())
    q->mlx.m_targetRate = dev->GetDataRate();
  q->m_rate = (q->m_rate / 2) + (q->mlx.m_targetRate / 2);
}

void RdmaCcDcqcn::HyperIncrease(Ptr<RdmaQueuePair> q) {
  uint32_t nic_idx = m_rdmaHw->GetNicIdxOfQp(q);
  Ptr<QbbNetDevice> dev = m_rdmaHw->m_nic[nic_idx].dev;
  q->mlx.m_targetRate += m_rhai;
  if (q->mlx.m_targetRate > dev->GetDataRate())
    q->mlx.m_targetRate = dev->GetDataRate();
  q->m_rate = (q->m_rate / 2) + (q->mlx.m_targetRate / 2);
}

} // namespace ns3
