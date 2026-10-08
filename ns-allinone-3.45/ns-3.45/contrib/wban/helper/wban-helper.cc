/*
 * Copyright (c) 2011 The Boeing Company
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Drishti Oza
 */
#include "wban-helper.h"

#include "ns3/log.h"
#include "ns3/mobility-model.h"
#include "ns3/multi-model-spectrum-channel.h"
#include "ns3/names.h"
#include "ns3/nstime.h"
#include "ns3/object.h"
#include "ns3/ptr.h"
#include "ns3/single-model-spectrum-channel.h"
#include "ns3/wban-error-model.h"
#include "ns3/wban-net-device.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("WbanHelper");

WbanHelper::WbanHelper()
{
    m_useMultiModelSpectrumChannel = false;
}

WbanHelper::WbanHelper(bool useMultiModelSpectrumChannel)
{
    m_useMultiModelSpectrumChannel = useMultiModelSpectrumChannel;
}

WbanHelper::~WbanHelper()
{
    m_channel->Dispose();
    m_channel = nullptr;
}

void
WbanHelper::EnableLogComponents()
{
    LogComponentEnableAll(LOG_PREFIX_TIME);
    LogComponentEnableAll(LOG_PREFIX_FUNC);
    LogComponentEnable("WbanErrorModel", LOG_LEVEL_ALL);
    LogComponentEnable("WbanInterferenceHelper", LOG_LEVEL_ALL);
    LogComponentEnable("WbanNetDevice", LOG_LEVEL_ALL);
    LogComponentEnable("WbanPhy", LOG_LEVEL_DEBUG);
    LogComponentEnable("WbanSpectrumSignalParameters", LOG_LEVEL_ALL);
    LogComponentEnable("WbanSpectrumValueHelper", LOG_LEVEL_ALL);
    LogComponentEnable("WbanPropagationLossModel", LOG_LEVEL_DEBUG);
}

void
WbanHelper::AddMobility(Ptr<wban::WbanPhy> phy, Ptr<MobilityModel> m)
{
    phy->SetMobility(m);

    // if mobility null give error
}

NetDeviceContainer
WbanHelper::Install(NodeContainer c)
{
    if (!m_channel)
    {
        if (m_useMultiModelSpectrumChannel)
        {
            m_channel = CreateObject<MultiModelSpectrumChannel>();
        }
        else
        {
            m_channel = CreateObject<SingleModelSpectrumChannel>();
        }

        if (!m_propagationDelay.IsTypeIdSet())
        {
            SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");
        }
        /* set body propagation as default loss model */
        if (m_propagationLoss.empty())
        {
            AddPropagationLossModel("ns3::BodyPropagationLossModel");
        }

        for (auto i = m_propagationLoss.begin(); i != m_propagationLoss.end(); ++i)
        {
            Ptr<PropagationLossModel> cur = (*i).Create<PropagationLossModel>();
            m_channel->AddPropagationLossModel(cur);
        }

        Ptr<PropagationDelayModel> delay = m_propagationDelay.Create<PropagationDelayModel>();
        m_channel->SetPropagationDelayModel(delay);
    }
    else
    {
        if (!m_channel->GetPropagationDelayModel())
        {
            NS_FATAL_ERROR("No propagation delay model added to the channel");
        }

        if (!m_channel->GetPropagationLossModel())
        {
            NS_FATAL_ERROR("No propagation loss model added to the channel");
        }
    }

    NetDeviceContainer devices;
    for (auto i = c.Begin(); i != c.End(); i++)
    {
        Ptr<Node> node = *i;

        Ptr<wban::WbanNetDevice> netDevice = CreateObject<wban::WbanNetDevice>();
        netDevice->SetChannel(m_channel);
        node->AddDevice(netDevice);
        netDevice->SetNode(node);
        devices.Add(netDevice);
    }
    return devices;
}

Ptr<SpectrumChannel>
WbanHelper::GetChannel()
{
    return m_channel;
}

void
WbanHelper::SetChannel(Ptr<SpectrumChannel> channel)
{
    m_channel = channel;
}

void
WbanHelper::SetChannel(std::string channelName)
{
    Ptr<SpectrumChannel> channel = Names::Find<SpectrumChannel>(channelName);
    m_channel = channel;
}

int64_t
WbanHelper::AssignStreams(NetDeviceContainer c, int64_t stream)
{
    int64_t currentStream = stream;
    Ptr<NetDevice> netDevice;
    for (auto i = c.Begin(); i != c.End(); ++i)
    {
        netDevice = (*i);
        Ptr<wban::WbanNetDevice> wban = DynamicCast<wban::WbanNetDevice>(netDevice);
        if (wban)
        {
            currentStream += wban->AssignStreams(currentStream);
        }
    }
    return (currentStream - stream);
}
} // namespace ns3
