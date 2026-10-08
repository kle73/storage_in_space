/*
 * Copyright (c) 2024 Tokushima University, Japan.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author:
 *  Alberto Gallegos Ramonet <alramonet@is.tokushima-u.ac.jp>
 */

#include "uart-lr-wpan-net-device.h"

#include "ns3/abort.h"
#include "ns3/boolean.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/pointer.h"
#include "ns3/spectrum-channel.h"

using namespace ns3::lrwpan;

namespace ns3
{
namespace uart
{

NS_LOG_COMPONENT_DEFINE("UartLrWpanNetDevice");
NS_OBJECT_ENSURE_REGISTERED(UartLrWpanNetDevice);

TypeId
UartLrWpanNetDevice::GetTypeId()
{
    static TypeId tid = TypeId("ns3::UartLrWpanNetDevice")
                            .SetParent<NetDevice>()
                            .SetGroupName("UartLrWpanNetDevice")
                            .AddConstructor<UartLrWpanNetDevice>();
    return tid;
}

UartLrWpanNetDevice::UartLrWpanNetDevice()
{
}

UartLrWpanNetDevice::UartLrWpanNetDevice(std::string port)
{
    NS_LOG_FUNCTION(this);
    m_mac = CreateObject<UartLrWpanMac>(port);
}

UartLrWpanNetDevice::~UartLrWpanNetDevice()
{
    NS_LOG_FUNCTION(this);
}

void
UartLrWpanNetDevice::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_mac = nullptr;
    m_node = nullptr;
    // chain up.
    NetDevice::DoDispose();
}

void
UartLrWpanNetDevice::DoInitialize()
{
    NS_LOG_FUNCTION(this);
    AggregateObject(m_mac);
    NetDevice::DoInitialize();
}

void
UartLrWpanNetDevice::SetMac(Ptr<UartLrWpanMac> mac)
{
    m_mac = mac;
}

Ptr<UartLrWpanMac>
UartLrWpanNetDevice::GetMac() const
{
    return m_mac;
}

void
UartLrWpanNetDevice::SetPanAssociation(uint16_t panId,
                                       Mac16Address coordShortAddr,
                                       Mac16Address assignedShortAddr)
{
}

void
UartLrWpanNetDevice::SetIfIndex(const uint32_t index)
{
    NS_LOG_FUNCTION(this << index);
    m_ifIndex = index;
}

uint32_t
UartLrWpanNetDevice::GetIfIndex() const
{
    NS_LOG_FUNCTION(this);
    return m_ifIndex;
}

Ptr<Channel>
UartLrWpanNetDevice::GetChannel() const
{
    NS_ABORT_MSG("Unsupported");
    return nullptr;
}

void
UartLrWpanNetDevice::LinkUp()
{
    NS_LOG_FUNCTION(this);
    m_linkUp = true;
}

void
UartLrWpanNetDevice::LinkDown()
{
    NS_LOG_FUNCTION(this);
    m_linkUp = false;
}

void
UartLrWpanNetDevice::MlmeSetConfirm(MlmeSetConfirmParams params)
{
    /*if (params.m_status == MacStatus::SUCCESS)
    {
        switch(params.id)
        {
            case lrwpan::MacPibAttributeIdentifier::macPanId:

        }
    }*/
}

void
UartLrWpanNetDevice::SetAddress(Address address)
{
    NS_LOG_FUNCTION(this);
    if (Mac16Address::IsMatchingType(address))
    {
        //  m_mac->SetShortAddress(Mac16Address::ConvertFrom(address));
    }
    else
    {
        NS_ABORT_MSG("UartLrWpanNetDevice::SetAddress - address is not of a compatible type");
    }
}

Address
UartLrWpanNetDevice::GetAddress() const
{
    NS_ABORT_MSG("Unsupported");
    return Mac16Address::GetBroadcast();
}

bool
UartLrWpanNetDevice::SetMtu(const uint16_t mtu)
{
    NS_ABORT_MSG("Unsupported");
    return false;
}

uint16_t
UartLrWpanNetDevice::GetMtu() const
{
    NS_LOG_FUNCTION(this);
    // Maximum payload size is: max psdu - frame control - seqno - addressing - security - fcs
    //                        = 127      - 2             - 1     - (2+2+2+2)  - 0        - 2
    //                        = 114
    // assuming no security and addressing with only 16 bit addresses without pan id compression.
    return 114;
}

bool
UartLrWpanNetDevice::IsLinkUp() const
{
    NS_ABORT_MSG("Unsupported");
    return false;
}

void
UartLrWpanNetDevice::AddLinkChangeCallback(Callback<void> callback)
{
    NS_LOG_FUNCTION(this);
    m_linkChanges.ConnectWithoutContext(callback);
}

bool
UartLrWpanNetDevice::IsBroadcast() const
{
    NS_LOG_FUNCTION(this);
    return true;
}

Address
UartLrWpanNetDevice::GetBroadcast() const
{
    NS_LOG_FUNCTION(this);
    return Mac16Address::GetBroadcast();
}

bool
UartLrWpanNetDevice::IsMulticast() const
{
    NS_ABORT_MSG("Unsupported");
    return false;
}

Address
UartLrWpanNetDevice::GetMulticast(Ipv4Address multicastGroup) const
{
    NS_ABORT_MSG("Unsupported");
    return Address();
}

Address
UartLrWpanNetDevice::GetMulticast(Ipv6Address addr) const
{
    NS_ABORT_MSG("Unsupported");
    return Mac16Address::GetBroadcast();
}

bool
UartLrWpanNetDevice::IsBridge() const
{
    NS_ABORT_MSG("Unsupported");
    return false;
}

bool
UartLrWpanNetDevice::IsPointToPoint() const
{
    NS_ABORT_MSG("Unsupported");
    return false;
}

bool
UartLrWpanNetDevice::Send(Ptr<Packet> packet, const Address& dest, uint16_t protocolNumber)
{
    return true;
}

bool
UartLrWpanNetDevice::SendFrom(Ptr<Packet> packet,
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
UartLrWpanNetDevice::GetNode() const
{
    NS_LOG_FUNCTION(this);
    return m_node;
}

void
UartLrWpanNetDevice::SetNode(Ptr<Node> node)
{
    NS_LOG_FUNCTION(this);
    m_node = node;
    m_mac->SetNodeId(m_node->GetId());
}

bool
UartLrWpanNetDevice::NeedsArp() const
{
    NS_LOG_FUNCTION(this);
    return false;
}

void
UartLrWpanNetDevice::SetReceiveCallback(ReceiveCallback cb)
{
    NS_LOG_FUNCTION(this);
    m_receiveCallback = cb;
}

void
UartLrWpanNetDevice::SetPromiscReceiveCallback(PromiscReceiveCallback cb)
{
    // This method basically assumes an 802.3-compliant device, but a raw
    // 802.15.4 device does not have an ethertype, and requires specific
    // McpsDataIndication parameters.
    // For further study:  how to support these methods somehow, such as
    // inventing a fake ethertype and packet tag for McpsDataRequest
    NS_LOG_WARN("Unsupported; use LrWpan MAC APIs instead");
}

bool
UartLrWpanNetDevice::SupportsSendFrom() const
{
    NS_LOG_FUNCTION_NOARGS();
    return false;
}

} // namespace uart
} // namespace ns3
