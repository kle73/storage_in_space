/*
 * Copyright (c) 2011 The Boeing Company
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Drishti Oza
 */
#include "wban-net-device.h"

#include "wban-error-model.h"
#include "wban-phy.h"

#include "ns3/abort.h"
#include "ns3/boolean.h"
#include "ns3/log.h"
#include "ns3/mobility-model.h"
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/pointer.h"
#include "ns3/spectrum-channel.h"

namespace ns3
{
namespace wban
{
NS_LOG_COMPONENT_DEFINE("WbanNetDevice");

NS_OBJECT_ENSURE_REGISTERED(WbanNetDevice);

TypeId
WbanNetDevice::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::WbanNetDevice")
            .SetParent<NetDevice>()
            .SetGroupName("Wban")
            .AddConstructor<WbanNetDevice>()
            .AddAttribute("Channel",
                          "The channel attached to this device",
                          PointerValue(),
                          MakePointerAccessor(&WbanNetDevice::DoGetChannel),
                          MakePointerChecker<SpectrumChannel>())
            .AddAttribute("Phy",
                          "The PHY layer attached to this device.",
                          PointerValue(),
                          MakePointerAccessor(&WbanNetDevice::GetPhy, &WbanNetDevice::SetPhy),
                          MakePointerChecker<WbanPhy>());
    return tid;
}

WbanNetDevice::WbanNetDevice()
    : m_configComplete(false)
{
    NS_LOG_FUNCTION(this);
    m_phy = CreateObject<WbanPhy>();
    CompleteConfig();
}

WbanNetDevice::~WbanNetDevice()
{
    NS_LOG_FUNCTION(this);
}

void
WbanNetDevice::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_phy->Dispose();
    m_phy = nullptr;
    m_node = nullptr;
    // chain up.
    NetDevice::DoDispose();
}

void
WbanNetDevice::DoInitialize()
{
    NS_LOG_FUNCTION(this);
    m_phy->Initialize();
    NetDevice::DoInitialize();
}

void
WbanNetDevice::CompleteConfig()
{
    NS_LOG_FUNCTION(this);
    if (!m_phy || !m_node || m_configComplete)
    {
        return;
    }

    // m_phy->SetPdDataIndicationCallback (MakeCallback (&WbanNetDevice::PdDataIndication, this));

    Ptr<MobilityModel> mobility = m_node->GetObject<MobilityModel>();
    if (!mobility)
    {
        NS_LOG_WARN("WbanNetDevice: no Mobility found on the node, probably it's not a good idea.");
    }
    m_phy->SetMobility(mobility);
    Ptr<WbanErrorModel> model = CreateObject<WbanErrorModel>();
    m_phy->SetErrorModel(model);
    m_phy->SetDevice(this);

    m_configComplete = true;
}

void
WbanNetDevice::SetPhy(Ptr<WbanPhy> phy)
{
    NS_LOG_FUNCTION(this);
    m_phy = phy;
    CompleteConfig();
}

void
WbanNetDevice::SetChannel(Ptr<SpectrumChannel> channel)
{
    NS_LOG_FUNCTION(this << channel);
    m_phy->SetChannel(channel);
    channel->AddRx(m_phy);
    CompleteConfig();
}

Ptr<WbanPhy>
WbanNetDevice::GetPhy() const
{
    NS_LOG_FUNCTION(this);
    return m_phy;
}

void
WbanNetDevice::SetIfIndex(const uint32_t index)
{
    NS_LOG_FUNCTION(this << index);
    m_ifIndex = index;
}

uint32_t
WbanNetDevice::GetIfIndex() const
{
    NS_LOG_FUNCTION(this);
    return m_ifIndex;
}

Ptr<Channel>
WbanNetDevice::GetChannel() const
{
    NS_LOG_FUNCTION(this);
    return m_phy->GetChannel();
}

void
WbanNetDevice::LinkUp()
{
    NS_LOG_FUNCTION(this);
    m_linkUp = true;
    m_linkChanges();
}

void
WbanNetDevice::LinkDown()
{
    NS_LOG_FUNCTION(this);
    m_linkUp = false;
    m_linkChanges();
}

Ptr<SpectrumChannel>
WbanNetDevice::DoGetChannel() const
{
    NS_LOG_FUNCTION(this);
    return m_phy->GetChannel();
}

void
WbanNetDevice::SetAddress(Address address)
{
}

Address
WbanNetDevice::GetAddress() const
{
    Ipv4Address b("255.255.255.255");
    return b;
}

bool
WbanNetDevice::SetMtu(const uint16_t mtu)
{
    NS_ABORT_MSG("Unsupported");
    return false;
}

uint16_t
WbanNetDevice::GetMtu() const
{
    NS_LOG_FUNCTION(this);
    // Maximum payload size is: max psdu - frame control - seqno - addressing - security - fcs
    //                        = 127      - 2             - 1     - (2+2+2+2)  - 0        - 2
    //                        = 114
    // assuming no security and addressing with only 16 bit addresses without pan id compression.
    return 114;
}

bool
WbanNetDevice::IsLinkUp() const
{
    NS_LOG_FUNCTION(this);
    return m_phy && m_linkUp;
}

void
WbanNetDevice::AddLinkChangeCallback(Callback<void> callback)
{
    NS_LOG_FUNCTION(this);
    m_linkChanges.ConnectWithoutContext(callback);
}

bool
WbanNetDevice::IsBroadcast() const
{
    NS_LOG_FUNCTION(this);
    return true;
}

Address
WbanNetDevice::GetBroadcast() const
{
    NS_LOG_FUNCTION(this);

    //  Address pseudoAddress = BuildPseudoMacAddress (m_mac->GetPanId (),
    //  Mac16Address::GetBroadcast ());
    Ipv4Address b("255.255.255.255");
    return b;
}

bool
WbanNetDevice::IsMulticast() const
{
    NS_LOG_FUNCTION(this);
    return true;
}

Address
WbanNetDevice::GetMulticast(Ipv4Address multicastGroup) const
{
    NS_ABORT_MSG("Unsupported");
    return Address();
}

Address
WbanNetDevice::GetMulticast(Ipv6Address addr) const
{
    NS_LOG_FUNCTION(this << addr);

    //  Mac48Address pseudoAddress = BuildPseudoMacAddress (m_mac->GetPanId (),
    //  Mac16Address::GetMulticast (addr));

    Ipv4Address b("255.255.255.255");
    return b;
}

bool
WbanNetDevice::IsBridge() const
{
    NS_LOG_FUNCTION(this);
    return false;
}

bool
WbanNetDevice::IsPointToPoint() const
{
    NS_LOG_FUNCTION(this);
    return false;
}

bool
WbanNetDevice::Send(Ptr<Packet> packet, const Address& dest, uint16_t protocolNumber)
{
    if (packet->GetSize() > GetMtu())
    {
        NS_LOG_ERROR("Fragmentation is needed for this packet, drop the packet ");
        return false;
    }

    return true;
}

bool
WbanNetDevice::SendFrom(Ptr<Packet> packet,
                        const Address& source,
                        const Address& dest,
                        uint16_t protocolNumber)
{
    NS_ABORT_MSG("Unsupported");
    // TODO: To support SendFrom, the MACs McpsDataRequest has to use the provided source address,
    // instead of to local one.
    return false;
}

Ptr<Node>
WbanNetDevice::GetNode() const
{
    NS_LOG_FUNCTION(this);
    return m_node;
}

void
WbanNetDevice::SetNode(Ptr<Node> node)
{
    NS_LOG_FUNCTION(this);
    m_node = node;
    CompleteConfig();
}

bool
WbanNetDevice::NeedsArp() const
{
    NS_LOG_FUNCTION(this);
    return true;
}

void
WbanNetDevice::SetReceiveCallback(ReceiveCallback cb)
{
    NS_LOG_FUNCTION(this);
    m_receiveCallback = cb;
}

void
WbanNetDevice::SetPromiscReceiveCallback(PromiscReceiveCallback cb)
{
    NS_LOG_WARN("Unsupported; use LrWpan MAC APIs instead");
}

bool
WbanNetDevice::SupportsSendFrom() const
{
    NS_LOG_FUNCTION_NOARGS();
    return false;
}

int64_t
WbanNetDevice::AssignStreams(int64_t stream)
{
    NS_LOG_FUNCTION(stream);
    int64_t streamIndex = stream;
    streamIndex += m_phy->AssignStreams(stream);
    NS_LOG_DEBUG("Number of assigned RV streams:  " << (streamIndex - stream));
    return (streamIndex - stream);
}
} // namespace wban
} // namespace ns3
