/*
 * Copyright (c) 2011 The Boeing Company
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Drishti Oza
 */
#include "wban-lqi-tag.h"

#include "ns3/integer.h"

namespace ns3
{
namespace wban
{
NS_OBJECT_ENSURE_REGISTERED(WbanLqiTag);

TypeId
WbanLqiTag::GetTypeId()
{
    static TypeId tid = TypeId("ns3::WbanLqiTag")
                            .SetParent<Tag>()
                            .SetGroupName("Wban")
                            .AddConstructor<WbanLqiTag>()
                            .AddAttribute("Lqi",
                                          "The lqi of the last packet received",
                                          IntegerValue(0),
                                          MakeIntegerAccessor(&WbanLqiTag::Get),
                                          MakeIntegerChecker<uint8_t>());
    return tid;
}

TypeId
WbanLqiTag::GetInstanceTypeId() const
{
    return GetTypeId();
}

WbanLqiTag::WbanLqiTag()
    : m_lqi(0)
{
}

WbanLqiTag::WbanLqiTag(uint8_t lqi)
    : m_lqi(lqi)
{
}

uint32_t
WbanLqiTag::GetSerializedSize() const
{
    return sizeof(uint8_t);
}

void
WbanLqiTag::Serialize(TagBuffer i) const
{
    i.WriteU8(m_lqi);
}

void
WbanLqiTag::Deserialize(TagBuffer i)
{
    m_lqi = i.ReadU8();
}

void
WbanLqiTag::Print(std::ostream& os) const
{
    os << "Lqi = " << m_lqi;
}

void
WbanLqiTag::Set(uint8_t lqi)
{
    m_lqi = lqi;
}

uint8_t
WbanLqiTag::Get() const
{
    return m_lqi;
}
} // namespace wban
} // namespace ns3
