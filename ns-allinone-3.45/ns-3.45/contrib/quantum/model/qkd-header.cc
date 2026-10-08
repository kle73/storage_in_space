/*
 * Copyright (c) 2025 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Anirudh V Gubbi <anirudhvgubbi@gmail.com>
 *          Akash Ravi <akashravi28055@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

#include "qkd-header.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("QkdHeader");
NS_OBJECT_ENSURE_REGISTERED(QkdHeader);

TypeId
QkdHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::QkdHeader")
                            .SetParent<Header>()
                            .SetGroupName("QKD")
                            .AddConstructor<QkdHeader>();
    return tid;
}

QkdHeader::QkdHeader()
    : m_phase(KEY_SIFTING)
{
}

void
QkdHeader::SetPhase(QkdPhase phase)
{
    m_phase = static_cast<uint8_t>(phase);
}

QkdPhase
QkdHeader::GetPhase() const
{
    return static_cast<QkdPhase>(m_phase);
}

TypeId
QkdHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

void
QkdHeader::Serialize(Buffer::Iterator start) const
{
    start.WriteU8(m_phase);
}

uint32_t
QkdHeader::GetSerializedSize() const
{
    return 1;
}

uint32_t
QkdHeader::Deserialize(Buffer::Iterator start)
{
    m_phase = start.ReadU8();
    return 1;
}

void
QkdHeader::Print(std::ostream& os) const
{
    os << "QkdPhase: " << static_cast<int>(m_phase);
}

} // namespace ns3
