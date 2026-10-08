/*
 * Copyright (c) 2011 The Boeing Company
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Drishti Oza
 */

#include "wban-channel.h"

#include "wban-net-device.h"
#include "wban-phy.h"

#include "ns3/log.h"
#include "ns3/mobility-model.h"
#include "ns3/node.h"
#include "ns3/pointer.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/simulator.h"

namespace ns3
{
namespace wban
{

NS_LOG_COMPONENT_DEFINE("WbanChannel");

NS_OBJECT_ENSURE_REGISTERED(WbanChannel);

TypeId
WbanChannel::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::WbanChannel")
            .SetParent<Channel>()
            .SetGroupName("Wifi")
            .AddConstructor<WbanChannel>()
            .AddAttribute("PropagationLossModel",
                          "A pointer to the propagation loss model attached to this channel.",
                          PointerValue(),
                          MakePointerAccessor(&WbanChannel::m_loss),
                          MakePointerChecker<PropagationLossModel>());
    return tid;
}

WbanChannel::WbanChannel()
{
    NS_LOG_FUNCTION(this);
}

WbanChannel::~WbanChannel()
{
    NS_LOG_FUNCTION(this);
    m_phyList.clear();
}

void
WbanChannel::SetWbanPropagationModel(const Ptr<PropagationLossModel> loss)
{
    NS_LOG_FUNCTION(this << loss);
    m_loss = loss;
}

std::size_t
WbanChannel::GetNDevices() const
{
    return m_phyList.size();
}

Ptr<NetDevice>
WbanChannel::GetDevice(std::size_t i) const
{
    return m_phyList[i]->GetDevice();
}

int64_t
WbanChannel::AssignStreams(int64_t stream)
{
    NS_LOG_FUNCTION(this << stream);
    int64_t currentStream = stream;
    currentStream += m_loss->AssignStreams(stream);
    return (currentStream - stream);
}
} // namespace wban

} // namespace ns3
