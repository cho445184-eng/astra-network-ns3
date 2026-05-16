#ifndef RDMA_CC_DCQCN_H
#define RDMA_CC_DCQCN_H

#include "rdma-congestion-ops.h"

namespace ns3 {

/**
 * \brief Mellanox DCQCN (Data Center QCN) congestion control for RDMA.
 *
 * Implements the rate-based CC algorithm used in Mellanox ConnectX NICs:
 * - Alpha update via periodic timer
 * - Rate decrease on CNP arrival
 * - Three-phase rate increase: fast recovery → active increase → hyper increase
 */
class RdmaCcDcqcn : public RdmaCongestionOps {
public:
  static TypeId GetTypeId();
  RdmaCcDcqcn();

  std::string GetName() const override { return "DCQCN"; }
  void InitQp(Ptr<RdmaQueuePair> qp, DataRate linkRate) override;
  void HandleAck(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch) override;
  void HandleCnp(Ptr<RdmaQueuePair> qp) override;
  void OnQpComplete(Ptr<RdmaQueuePair> qp) override;
  Ptr<RdmaCongestionOps> Fork() override;

private:
  void UpdateAlpha(Ptr<RdmaQueuePair> q);
  void ScheduleUpdateAlpha(Ptr<RdmaQueuePair> q);
  void CheckRateDecrease(Ptr<RdmaQueuePair> q);
  void ScheduleDecreaseRate(Ptr<RdmaQueuePair> q, uint32_t delta);
  void RateIncEventTimer(Ptr<RdmaQueuePair> q);
  void RateIncEvent(Ptr<RdmaQueuePair> q);
  void FastRecovery(Ptr<RdmaQueuePair> q);
  void ActiveIncrease(Ptr<RdmaQueuePair> q);
  void HyperIncrease(Ptr<RdmaQueuePair> q);

  double m_g;                    // feedback weight (EWMA gain)
  double m_rateOnFirstCNP;       // fraction of line rate on first CNP
  bool m_ecnClampTgtRate;        // clamp target rate
  double m_rpgTimeReset;         // rate increase timer reset (us)
  double m_rateDecreaseInterval; // rate decrease check interval (us)
  uint32_t m_rpgThreshold;       // fast recovery times threshold
  double m_alphaResumeInterval;  // alpha update interval (us)
  DataRate m_rai;                // additive increase rate
  DataRate m_rhai;               // hyper-additive increase rate
  DataRate m_minRate;            // minimum rate
};

} // namespace ns3

#endif // RDMA_CC_DCQCN_H
