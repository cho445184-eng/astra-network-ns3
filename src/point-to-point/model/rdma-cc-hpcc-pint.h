#ifndef RDMA_CC_HPCC_PINT_H
#define RDMA_CC_HPCC_PINT_H

#include "rdma-congestion-ops.h"

namespace ns3 {

/**
 * \brief HPCC-PINT congestion control for RDMA.
 *
 * Compressed variant of HPCC using Probabilistic INT (PINT)
 * for bandwidth-efficient telemetry.
 */
class RdmaCcHpccPint : public RdmaCongestionOps {
public:
  static TypeId GetTypeId();
  RdmaCcHpccPint();

  std::string GetName() const override { return "HPCC-PINT"; }
  void InitQp(Ptr<RdmaQueuePair> qp, DataRate linkRate) override;
  void HandleAck(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch) override;
  void HandleCnp(Ptr<RdmaQueuePair> qp) override;
  void OnQpComplete(Ptr<RdmaQueuePair> qp) override;
  Ptr<RdmaCongestionOps> Fork() override;

  void SetSampleThreshold(double p);

private:
  void UpdateRate(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch, bool fast_react);

  double m_targetUtil;
  uint32_t m_miThresh;
  uint32_t m_smplThresh;
  DataRate m_rai;
  DataRate m_minRate;
};

} // namespace ns3

#endif // RDMA_CC_HPCC_PINT_H
