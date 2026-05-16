#ifndef RDMA_CC_HPCC_H
#define RDMA_CC_HPCC_H

#include "rdma-congestion-ops.h"

namespace ns3 {

/**
 * \brief HPCC (High Precision Congestion Control) for RDMA.
 *
 * Uses multi-hop In-band Network Telemetry (INT) to compute
 * per-hop utilization and adjust the sending rate precisely.
 */
class RdmaCcHpcc : public RdmaCongestionOps {
public:
  static TypeId GetTypeId();
  RdmaCcHpcc();

  std::string GetName() const override { return "HPCC"; }
  void InitQp(Ptr<RdmaQueuePair> qp, DataRate linkRate) override;
  void HandleAck(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch) override;
  void HandleCnp(Ptr<RdmaQueuePair> qp) override;
  void OnQpComplete(Ptr<RdmaQueuePair> qp) override;
  Ptr<RdmaCongestionOps> Fork() override;

private:
  void UpdateRate(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch, bool fast_react);
  void FastReact(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch);

  double m_targetUtil;
  double m_utilHigh;
  uint32_t m_miThresh;
  bool m_multipleRate;
  bool m_sampleFeedback;
  bool m_fastReact;
  DataRate m_rai;
  DataRate m_minRate;
};

} // namespace ns3

#endif // RDMA_CC_HPCC_H
