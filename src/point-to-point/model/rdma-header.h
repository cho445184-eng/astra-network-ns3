#ifndef RDMA_HEADER_H
#define RDMA_HEADER_H

#include "ns3/header.h"
#include "ns3/int-header.h"

namespace ns3 {

/**
 * \brief Standard ns-3 Header for RDMA data packets.
 *
 * Encapsulates the RDMA-specific fields that ride on top of IPv4/UDP:
 * sequence number, priority group, and optional INT header.
 *
 * This is a clean Header-pattern replacement for the ad-hoc combination
 * of SeqTsHeader + UdpHeader currently used in RdmaHw::GetNxtPacket.
 *
 * Wire format (after UDP header):
 *   [4 bytes] Sequence number
 *   [2 bytes] Priority group (pg)
 *   [variable] IntHeader (size depends on IntHeader::mode)
 */
class RdmaDataHeader : public Header {
public:
  static TypeId GetTypeId();
  TypeId GetInstanceTypeId() const override;

  RdmaDataHeader();
  ~RdmaDataHeader() override;

  void SetSeq(uint32_t seq);
  uint32_t GetSeq() const;

  void SetPG(uint16_t pg);
  uint16_t GetPG() const;

  IntHeader& GetIntHeader() { return m_ih; }
  const IntHeader& GetIntHeader() const { return m_ih; }

  // Header interface
  uint32_t GetSerializedSize() const override;
  void Serialize(Buffer::Iterator start) const override;
  uint32_t Deserialize(Buffer::Iterator start) override;
  void Print(std::ostream &os) const override;

private:
  uint32_t m_seq;
  uint16_t m_pg;
  IntHeader m_ih;
};

/**
 * \brief Standard ns-3 Header for RDMA ACK/NACK packets.
 *
 * Encapsulates ACK/NACK fields:
 *   sequence, priority group, source/dest ports, flags, and INT header.
 *
 * Wire format:
 *   [4 bytes] Sequence number (ACK or NACK seq)
 *   [2 bytes] Priority group
 *   [2 bytes] Source port
 *   [2 bytes] Destination port
 *   [1 byte]  Flags (bit 0 = CNP)
 *   [variable] IntHeader
 */
class RdmaAckHeader : public Header {
public:
  static const uint8_t FLAG_CNP = 0;

  static TypeId GetTypeId();
  TypeId GetInstanceTypeId() const override;

  RdmaAckHeader();
  ~RdmaAckHeader() override;

  void SetSeq(uint32_t seq);
  uint32_t GetSeq() const;

  void SetPG(uint16_t pg);
  uint16_t GetPG() const;

  void SetSport(uint16_t sport);
  uint16_t GetSport() const;

  void SetDport(uint16_t dport);
  uint16_t GetDport() const;

  void SetCnp();
  bool IsCnp() const;

  void SetFlags(uint8_t flags);
  uint8_t GetFlags() const;

  void SetIntHeader(const IntHeader &ih);
  IntHeader& GetIntHeader() { return m_ih; }
  const IntHeader& GetIntHeader() const { return m_ih; }

  // Header interface
  uint32_t GetSerializedSize() const override;
  void Serialize(Buffer::Iterator start) const override;
  uint32_t Deserialize(Buffer::Iterator start) override;
  void Print(std::ostream &os) const override;

private:
  uint32_t m_seq;
  uint16_t m_pg;
  uint16_t m_sport;
  uint16_t m_dport;
  uint8_t m_flags;
  IntHeader m_ih;
};

} // namespace ns3

#endif // RDMA_HEADER_H
