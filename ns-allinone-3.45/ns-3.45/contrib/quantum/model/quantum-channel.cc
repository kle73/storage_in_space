/*
 * Copyright (c) 2025 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Satyam Shukla <shuklasatyam774@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

#include "quantum-channel.h"

#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/simulator.h"

#include <iterator>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("QuantumChannel");

NS_OBJECT_ENSURE_REGISTERED(QuantumChannel);

TypeId
QuantumChannel::GetTypeId()
{
    static TypeId tid = TypeId("ns3::QuantumChannel")
                            .SetParent<Channel>()
                            .SetGroupName("Network")
                            .AddConstructor<QuantumChannel>();
    return tid;
}

QuantumChannel::QuantumChannel()
{
    m_devices.first.second = nullptr;
    m_devices.second.second = nullptr;
    NS_LOG_FUNCTION(this);
}

void
QuantumChannel::Send(Ptr<QBit> qbit, Mac48Address senderId)
{
    NS_LOG_FUNCTION(this << senderId);
    if (senderId == m_devices.first.first)
    {
        Ptr<QuantumDevice> tmp = m_devices.second.second;
        Simulator::ScheduleWithContext(tmp->GetNode()->GetId(),
                                       m_trustedDevicesDelay,
                                       &QuantumDevice::ReceiveQubit,
                                       m_devices.second.second,
                                       qbit,
                                       senderId);
        std::map<Mac48Address, QuantumDeviceDelayPair>::iterator it;
        for (it = m_delays.begin(); it != m_delays.end(); ++it)
        {
            tmp = it->second.first;
            Time delay = it->second.second;
            Simulator::ScheduleWithContext(tmp->GetNode()->GetId(),
                                           delay,
                                           &QuantumDevice::ReceiveQubit,
                                           tmp,
                                           qbit,
                                           senderId);
        }
    }
    else if (senderId == m_devices.second.first)
    {
        Ptr<QuantumDevice> tmp = m_devices.first.second;
        Simulator::ScheduleWithContext(tmp->GetNode()->GetId(),
                                       m_trustedDevicesDelay,
                                       &QuantumDevice::ReceiveQubit,
                                       m_devices.first.second,
                                       qbit,
                                       senderId);
        std::map<Mac48Address, QuantumDeviceDelayPair>::iterator it;
        for (it = m_delays.begin(); it != m_delays.end(); ++it)
        {
            tmp = it->second.first;
            Time delay = it->second.second;
            if ((m_trustedDevicesDelay - delay).GetSeconds() > 0)
            {
                Simulator::ScheduleWithContext(tmp->GetNode()->GetId(),
                                               m_trustedDevicesDelay - delay,
                                               &QuantumDevice::ReceiveQubit,
                                               tmp,
                                               qbit,
                                               senderId);
            }
        }
    }
    else
    {
        NS_LOG_ERROR("Sender is not connected to the channel");
    }
}

void
QuantumChannel::AddEavesdropper(Mac48Address deviceId, Ptr<QuantumDevice> device, Time delay)
{
    NS_LOG_FUNCTION(this << deviceId << delay);
    if (this->m_delays.find(deviceId) == m_delays.end())
    {
        QuantumDeviceDelayPair pair;
        pair.first = device;
        pair.second = delay;
        m_delays.insert(std::make_pair(deviceId, pair));
    }
    else
    {
        NS_LOG_WARN("Eavesdropper already added");
    }
}

Time
QuantumChannel::GetDelay(Mac48Address deviceId)
{
    NS_LOG_FUNCTION(this << deviceId);
    if (deviceId == m_devices.first.first || deviceId == m_devices.second.first)
    {
        return m_trustedDevicesDelay;
    }
    else if (this->m_delays.find(deviceId) == m_delays.end())
    {
        NS_LOG_ERROR("Device not found");
        return Seconds(0);
    }
    return m_delays[deviceId].second;
}

void
QuantumChannel::SetDelay(Mac48Address deviceId, Time t)
{
    NS_LOG_FUNCTION(this << deviceId << t);
    if (deviceId == m_devices.first.first || deviceId == m_devices.second.first)
    {
        m_trustedDevicesDelay = t;
    }
    else if (this->m_delays.find(deviceId) == m_delays.end())
    {
        NS_LOG_ERROR("Device not found");
        return;
    }
    m_delays[deviceId].second = t;
}

void
QuantumChannel::AddDevice(Mac48Address deviceId, Ptr<QuantumDevice> device, Time delay)
{
    NS_LOG_FUNCTION(this << device << delay);
    if (m_devices.first.second == nullptr)
    {
        m_devices.first.second = device;
        m_devices.first.first = deviceId;
        this->m_trustedDevicesDelay = delay;
    }
    else if (m_devices.second.second == nullptr)
    {
        m_devices.second.second = device;
        m_devices.second.first = deviceId;
        this->m_trustedDevicesDelay = delay;
    }
    else
    {
        NS_LOG_ERROR("Two devices are already connected");
    }
}

std::size_t
QuantumChannel::GetNDevices() const
{
    NS_LOG_FUNCTION(this);
    std::size_t n = 0;
    if (m_devices.first.second != nullptr)
    {
        n++;
    }
    if (m_devices.second.second != nullptr)
    {
        n++;
    }
    n += m_delays.size();
    return n;
}

Ptr<NetDevice>
QuantumChannel::GetDevice(std::size_t i) const
{
    NS_LOG_FUNCTION(this << i);
    if (i == 0)
    {
        return m_devices.first.second;
    }
    else if (i == 1)
    {
        return m_devices.second.second;
    }
    else if (i < m_delays.size() + 2)
    {
        std::map<Mac48Address, QuantumDeviceDelayPair>::const_iterator it = m_delays.begin();
        std::advance(it, i - 2);
        return it->second.first;
    }
    else
    {
        NS_LOG_ERROR("Device not found");
        return nullptr;
    }
}

} // namespace ns3
