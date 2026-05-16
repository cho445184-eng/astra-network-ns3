#ifndef RDMA_L4_PROTOCOL_H
#define RDMA_L4_PROTOCOL_H

#include <ns3/ip-l4-protocol.h>
#include <ns3/ipv4-address.h>
#include <ns3/node.h>
#include <ns3/packet.h>

namespace ns3 {

class RdmaHw;

/**
 * \brief RDMA transport layer protocol following ns-3's IpL4Protocol pattern.
 *
 * This class bridges the gap between ns-3's standard L3/L4 layering
 * and the RDMA simulation stack. It registers with Ipv4L3Protocol
 * using protocol numbers 0xFC (ACK), 0xFD (NACK), 0xFF (CNP), and
 * 0x11 (UDP, for RDMA data), enabling packets to flow through the
 * standard Ipv4 receive path instead of being intercepted at L2.
 *
 * The protocol delegates actual processing to RdmaHw via callbacks,
 * maintaining the existing simulation behavior while conforming to
 * the standard ns-3 protocol stack architecture.
 *
 * Protocol numbers used by the RDMA stack:
 *   0x11 (17)  - UDP: RDMA data packets
 *   0xFC (252) - ACK packets
 *   0xFD (253) - NACK packets
 *   0xFE (254) - PFC (Priority Flow Control) — handled at L2
 *   0xFF (255) - CNP (Congestion Notification Packets)
 */
class RdmaL4Protocol : public IpL4Protocol {
public:
  static const uint8_t PROT_NUMBER_ACK = 0xFC;
  static const uint8_t PROT_NUMBER_NACK = 0xFD;
  static const uint8_t PROT_NUMBER_CNP = 0xFF;

  static TypeId GetTypeId();
  RdmaL4Protocol();
  ~RdmaL4Protocol() override;

  int GetProtocolNumber() const override;

  // IPv4 receive path
  IpL4Protocol::RxStatus Receive(Ptr<Packet> p,
                                  const Ipv4Header &header,
                                  Ptr<Ipv4Interface> incomingInterface) override;

  // IPv6 receive path (not used in RDMA simulation)
  IpL4Protocol::RxStatus Receive(Ptr<Packet> p,
                                  const Ipv6Header &header,
                                  Ptr<Ipv6Interface> incomingInterface) override;

  void SetDownTarget(IpL4Protocol::DownTargetCallback cb) override;
  void SetDownTarget6(IpL4Protocol::DownTargetCallback6 cb) override;
  IpL4Protocol::DownTargetCallback GetDownTarget() const override;
  IpL4Protocol::DownTargetCallback6 GetDownTarget6() const override;

  /**
   * Send a packet down to L3.
   * Wraps the down-target callback, providing a clean API for
   * RDMA components to send packets through the standard stack.
   */
  void SendDown(Ptr<Packet> p, Ipv4Address src, Ipv4Address dst, uint8_t protocol);

  void SetRdmaHw(Ptr<RdmaHw> hw);
  void SetNode(Ptr<Node> node);

protected:
  void NotifyNewAggregate() override;

private:
  Ptr<Node> m_node;
  Ptr<RdmaHw> m_rdmaHw;
  IpL4Protocol::DownTargetCallback m_downTarget;
  IpL4Protocol::DownTargetCallback6 m_downTarget6;
};

} // namespace ns3

#endif // RDMA_L4_PROTOCOL_H
