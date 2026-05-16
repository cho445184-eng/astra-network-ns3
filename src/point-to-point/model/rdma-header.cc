#include "rdma-header.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("RdmaHeader");

// ==================== RdmaDataHeader ====================

NS_OBJECT_ENSURE_REGISTERED(RdmaDataHeader);

TypeId RdmaDataHeader::GetTypeId() {
  static TypeId tid = TypeId("ns3::RdmaDataHeader")
    .SetParent<Header>()
    .SetGroupName("PointToPoint")
    .AddConstructor<RdmaDataHeader>();
  return tid;
}

TypeId RdmaDataHeader::GetInstanceTypeId() const {
  return GetTypeId();
}

RdmaDataHeader::RdmaDataHeader() : m_seq(0), m_pg(0) {}
RdmaDataHeader::~RdmaDataHeader() {}

void RdmaDataHeader::SetSeq(uint32_t seq) { m_seq = seq; }
uint32_t RdmaDataHeader::GetSeq() const { return m_seq; }

void RdmaDataHeader::SetPG(uint16_t pg) { m_pg = pg; }
uint16_t RdmaDataHeader::GetPG() const { return m_pg; }

uint32_t RdmaDataHeader::GetSerializedSize() const {
  return 4 + 2 + IntHeader::GetStaticSize();
}

void RdmaDataHeader::Serialize(Buffer::Iterator start) const {
  Buffer::Iterator i = start;
  i.WriteHtonU32(m_seq);
  i.WriteHtonU16(m_pg);
  m_ih.Serialize(i);
}

uint32_t RdmaDataHeader::Deserialize(Buffer::Iterator start) {
  Buffer::Iterator i = start;
  m_seq = i.ReadNtohU32();
  m_pg = i.ReadNtohU16();
  m_ih.Deserialize(i);
  return GetSerializedSize();
}

void RdmaDataHeader::Print(std::ostream &os) const {
  os << "RdmaDataHeader(seq=" << m_seq << " pg=" << m_pg << ")";
}

// ==================== RdmaAckHeader ====================

NS_OBJECT_ENSURE_REGISTERED(RdmaAckHeader);

TypeId RdmaAckHeader::GetTypeId() {
  static TypeId tid = TypeId("ns3::RdmaAckHeader")
    .SetParent<Header>()
    .SetGroupName("PointToPoint")
    .AddConstructor<RdmaAckHeader>();
  return tid;
}

TypeId RdmaAckHeader::GetInstanceTypeId() const {
  return GetTypeId();
}

RdmaAckHeader::RdmaAckHeader()
  : m_seq(0), m_pg(0), m_sport(0), m_dport(0), m_flags(0) {}
RdmaAckHeader::~RdmaAckHeader() {}

void RdmaAckHeader::SetSeq(uint32_t seq) { m_seq = seq; }
uint32_t RdmaAckHeader::GetSeq() const { return m_seq; }

void RdmaAckHeader::SetPG(uint16_t pg) { m_pg = pg; }
uint16_t RdmaAckHeader::GetPG() const { return m_pg; }

void RdmaAckHeader::SetSport(uint16_t sport) { m_sport = sport; }
uint16_t RdmaAckHeader::GetSport() const { return m_sport; }

void RdmaAckHeader::SetDport(uint16_t dport) { m_dport = dport; }
uint16_t RdmaAckHeader::GetDport() const { return m_dport; }

void RdmaAckHeader::SetCnp() { m_flags |= (1 << FLAG_CNP); }
bool RdmaAckHeader::IsCnp() const { return (m_flags >> FLAG_CNP) & 1; }

void RdmaAckHeader::SetFlags(uint8_t flags) { m_flags = flags; }
uint8_t RdmaAckHeader::GetFlags() const { return m_flags; }

void RdmaAckHeader::SetIntHeader(const IntHeader &ih) { m_ih = ih; }

uint32_t RdmaAckHeader::GetSerializedSize() const {
  return 4 + 2 + 2 + 2 + 1 + IntHeader::GetStaticSize();
}

void RdmaAckHeader::Serialize(Buffer::Iterator start) const {
  Buffer::Iterator i = start;
  i.WriteHtonU32(m_seq);
  i.WriteHtonU16(m_pg);
  i.WriteHtonU16(m_sport);
  i.WriteHtonU16(m_dport);
  i.WriteU8(m_flags);
  m_ih.Serialize(i);
}

uint32_t RdmaAckHeader::Deserialize(Buffer::Iterator start) {
  Buffer::Iterator i = start;
  m_seq = i.ReadNtohU32();
  m_pg = i.ReadNtohU16();
  m_sport = i.ReadNtohU16();
  m_dport = i.ReadNtohU16();
  m_flags = i.ReadU8();
  m_ih.Deserialize(i);
  return GetSerializedSize();
}

void RdmaAckHeader::Print(std::ostream &os) const {
  os << "RdmaAckHeader(seq=" << m_seq << " pg=" << m_pg
     << " sport=" << m_sport << " dport=" << m_dport
     << " cnp=" << IsCnp() << ")";
}

} // namespace ns3
