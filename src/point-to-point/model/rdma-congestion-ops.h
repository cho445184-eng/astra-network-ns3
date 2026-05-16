#ifndef RDMA_CONGESTION_OPS_H
#define RDMA_CONGESTION_OPS_H

#include <ns3/object.h>
#include <ns3/data-rate.h>
#include <ns3/packet.h>
#include <ns3/custom-header.h>
#include <ns3/rdma-queue-pair.h>

namespace ns3 {

class RdmaHw;
// Raw pointer used to avoid circular Ptr<> dependency (RdmaHw includes this header)

/**
 * \brief Abstract base class for RDMA congestion control algorithms.
 *
 * Follows the same pluggable pattern as TcpCongestionOps:
 * each algorithm is a subclass of RdmaCongestionOps, registered
 * via ns-3's TypeId system and selected via the RdmaHw "CcOps" attribute.
 *
 * Lifecycle:
 *  1. Factory creates the ops object; SetRdmaHw() is called once.
 *  2. InitQp() is called when a new QP is created.
 *  3. HandleAck() / HandleCnp() are called on congestion signals.
 *  4. OnQpComplete() is called when the QP finishes.
 */
class RdmaCongestionOps : public Object {
public:
  static TypeId GetTypeId();
  RdmaCongestionOps();
  ~RdmaCongestionOps() override;

  virtual std::string GetName() const = 0;

  /**
   * Back-pointer to the owning RdmaHw (set once during setup).
   * Subclasses may need this to access NIC information (e.g. link rate).
   */
  virtual void SetRdmaHw(RdmaHw* hw);

  /**
   * Initialize congestion-control state for a newly created QP.
   * Called from RdmaHw::AddQueuePair after the QP is registered.
   * \param qp The queue pair being initialized.
   * \param linkRate The link rate of the NIC this QP is bound to.
   */
  virtual void InitQp(Ptr<RdmaQueuePair> qp, DataRate linkRate) = 0;

  /**
   * Handle an ACK (or NACK) for the given QP.
   * This is the main congestion-control entry point, called from
   * RdmaHw::ReceiveAck after reliability processing.
   * \param qp The sender-side queue pair.
   * \param p The received ACK/NACK packet.
   * \param ch Parsed custom header (contains INT, ECN flags, etc.).
   */
  virtual void HandleAck(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch) = 0;

  /**
   * Handle a CNP (Congestion Notification Packet) for the given QP.
   * Called when the ACK carries a CNP flag, or when a standalone CNP arrives.
   * \param qp The sender-side queue pair.
   */
  virtual void HandleCnp(Ptr<RdmaQueuePair> qp) = 0;

  /**
   * Cleanup when a QP is complete (cancel timers, etc.).
   * \param qp The queue pair that finished.
   */
  virtual void OnQpComplete(Ptr<RdmaQueuePair> qp) = 0;

  /**
   * Clone this CC ops instance (factory pattern for per-QP state if needed).
   */
  virtual Ptr<RdmaCongestionOps> Fork() = 0;

protected:
  RdmaHw* m_rdmaHw;
};

} // namespace ns3

#endif // RDMA_CONGESTION_OPS_H
