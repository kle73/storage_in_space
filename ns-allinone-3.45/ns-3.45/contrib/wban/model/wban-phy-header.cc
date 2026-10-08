/*
 * Copyright (c) 2011 The Boeing Company
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Drishti Oza
 */
#include "wban-phy-header.h"

#include "ns3/abort.h"
#include "ns3/antenna-model.h"
#include "ns3/double.h"
#include "ns3/header.h"
#include "ns3/log.h"
#include "ns3/mobility-model.h"
#include "ns3/net-device.h"
#include "ns3/packet-burst.h"
#include "ns3/packet.h"
#include "ns3/random-variable-stream.h"
#include "ns3/simulator.h"
#include "ns3/spectrum-channel.h"
#include "ns3/spectrum-value.h"

namespace ns3
{
namespace wban
{
NS_OBJECT_ENSURE_REGISTERED(NarrowBandPhyHeader);

/**
 * Frame structure for NarrowBand
 * see Fig 116 IEEE 802.15.6
 */
uint32_t
NarrowBandPhyHeader::GetFrame() const
{
    uint32_t val = 0;
    val = m_phyHeaderRate & (0x07);                     // Bit 0-2
    val |= (m_phyHeaderLength << 4) & (0xFF << 4);      // Bit 4-11
    val |= (m_phyHeaderBurstMode << 13) & (0x01 << 13); // Bit 13
    val |= (m_phyHeaderSeed << 14) & (0x01 << 14);      // Bit 14
    val |= (m_headerHcs << 15) & (0x0F << 15);          // 4 bits
    val |= (m_headerBchParity << 19) & (0xFFF << 19);   // 12 bits
    return val;
}

uint32_t
NarrowBandPhyHeader::GetRate() const
{
    return (m_phyHeaderRate);
}

uint32_t
NarrowBandPhyHeader::GetLength() const
{
    return (m_phyHeaderLength);
}

bool
NarrowBandPhyHeader::IsBurst() const
{
    return (m_phyHeaderBurstMode == 1);
}

bool
NarrowBandPhyHeader::IsSeed() const
{
    return (m_phyHeaderSeed == 1);
}

uint8_t
NarrowBandPhyHeader::GetHcs() const
{
    return (m_headerHcs);
}

uint32_t
NarrowBandPhyHeader::GetBchParity() const
{
    return (m_headerBchParity);
}

void
NarrowBandPhyHeader::SetFrame(uint32_t frame)
{
    m_phyHeaderRate = (frame) & (0x07);            // Bit 0-2
    m_phyHeaderLength = (frame >> 4) & (0xFF);     // Bit 4-11
    m_phyHeaderBurstMode = (frame >> 13) & (0x01); // Bit 13
    m_phyHeaderSeed = (frame >> 14) & (0x01);      // Bit 14
    m_headerHcs = (frame >> 15) & (0x0F);          // 4 bits
    m_headerBchParity = (frame >> 19) & (0xFFF);   // 12 bits
}

void
NarrowBandPhyHeader::SetRate(uint32_t rate)
{
    m_phyHeaderRate = rate;
}

void
NarrowBandPhyHeader::SetLength(uint32_t length)
{
    m_phyHeaderLength = length;
}

void
NarrowBandPhyHeader::SetBurstEnable()
{
    m_phyHeaderBurstMode = true;
}

void
NarrowBandPhyHeader::SetBurstDisable()
{
    m_phyHeaderBurstMode = false;
}

void
NarrowBandPhyHeader::SetSeedEnable()
{
    m_phyHeaderSeed = true;
}

void
NarrowBandPhyHeader::SetSeedDisable()
{
    m_phyHeaderSeed = false;
}

void
NarrowBandPhyHeader::SetHcs(uint8_t hcs)
{
    m_headerHcs = hcs;
}

void
NarrowBandPhyHeader::SetBchParity(uint32_t parity)
{
    m_headerBchParity = parity;
}

TypeId
NarrowBandPhyHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::NarrowBandPhyHeader")
                            .SetParent<Header>()
                            .SetGroupName("Wban")
                            .AddConstructor<NarrowBandPhyHeader>();
    return tid;
}

TypeId
NarrowBandPhyHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
NarrowBandPhyHeader::GetSerializedSize() const
{
    return 3;
}

void
NarrowBandPhyHeader::Serialize(Buffer::Iterator i) const
{
    uint16_t nbFrame = GetFrame();
    i.WriteHtolsbU16(nbFrame);
}

uint32_t
NarrowBandPhyHeader::Deserialize(Buffer::Iterator i)
{
    uint16_t nbFrame = i.ReadLsbtohU16();
    SetFrame(nbFrame);
    return i.GetDistanceFrom(i);
}

void
NarrowBandPhyHeader::Print(std::ostream& os) const
{
    os << "NarrowBand PACKET  : "
       << "  Header rate = " << (uint8_t)m_phyHeaderRate
       << ", Header Length = " << (uint8_t)m_phyHeaderLength
       << ", Burst Mode = " << (bool)m_phyHeaderBurstMode << ", seed = " << (bool)m_phyHeaderSeed
       << ", HCS = " << (uint8_t)m_headerHcs << ", BCH Parity= " << (uint32_t)m_headerBchParity;
}
} // namespace wban
} // namespace ns3
