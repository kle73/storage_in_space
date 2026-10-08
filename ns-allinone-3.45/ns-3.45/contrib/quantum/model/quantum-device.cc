/*
 * Copyright (c) 2025 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Satyam Shukla <shuklasatyam774@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */
#include "quantum-device.h"

#include "quantum-channel.h"

#include "ns3/node.h"
#include "ns3/simulator.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("QuantumDevice");
NS_OBJECT_ENSURE_REGISTERED(QuantumDevice);

TypeId
QuantumDevice::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::QuantumDevice").SetParent<SimpleNetDevice>();
    return tid;
}

QuantumDevice::~QuantumDevice()
{
    NS_LOG_FUNCTION(this);
}

void
QuantumDevice::SetQuantumChannel(Ptr<QuantumChannel> channel)
{
    NS_LOG_FUNCTION(this << channel);
    m_channel = channel;
}

Ptr<QuantumChannel>
QuantumDevice::GetQuantumChannel() const
{
    NS_LOG_FUNCTION(this);
    return m_channel;
}

} // namespace ns3
