#ifndef RDMA_CC_TIMELY_H
#define RDMA_CC_TIMELY_H

#include "rdma-congestion-ops.h"

namespace ns3 {

/**
 * \brief TIMELY congestion control for RDMA.
 *
 * RTT-based rate control using delay gradient estimation.
 */
class RdmaCcTimely : public RdmaCongestionOps {
public:
  static TypeId GetTypeId();
  RdmaCcTimely();

  std::string GetName() const override { return "TIMELY"; }
  void InitQp(Ptr<RdmaQueuePair> qp, DataRate linkRate) override;
  void HandleAck(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch) override;
  void HandleCnp(Ptr<RdmaQueuePair> qp) override;
  void OnQpComplete(Ptr<RdmaQueuePair> qp) override;
  Ptr<RdmaCongestionOps> Fork() override;

private:
  void UpdateRate(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch, bool us);

  double m_alpha;
  double m_beta;
  uint64_t m_tLow;
  uint64_t m_tHigh;
  uint64_t m_minRtt;
  DataRate m_rai;
  DataRate m_rhai;
  DataRate m_minRate;
};

} // namespace ns3

#endif // RDMA_CC_TIMELY_H
