#ifndef RDMA_CC_DCTCP_H
#define RDMA_CC_DCTCP_H

#include "rdma-congestion-ops.h"

namespace ns3 {

/**
 * \brief DCTCP congestion control for RDMA.
 *
 * Rate-based adaptation of DCTCP: uses ECN fraction to compute alpha,
 * then multiplicative decrease by (1 - alpha/2) when ECN is received.
 */
class RdmaCcDctcp : public RdmaCongestionOps {
public:
  static TypeId GetTypeId();
  RdmaCcDctcp();

  std::string GetName() const override { return "DCTCP"; }
  void InitQp(Ptr<RdmaQueuePair> qp, DataRate linkRate) override;
  void HandleAck(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch) override;
  void HandleCnp(Ptr<RdmaQueuePair> qp) override;
  void OnQpComplete(Ptr<RdmaQueuePair> qp) override;
  Ptr<RdmaCongestionOps> Fork() override;

private:
  double m_g;
  DataRate m_rai;
  DataRate m_minRate;
  uint32_t m_mtu;
};

} // namespace ns3

#endif // RDMA_CC_DCTCP_H
