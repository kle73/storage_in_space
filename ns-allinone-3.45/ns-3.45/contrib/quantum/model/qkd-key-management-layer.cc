/**
 * Copyright (c) 2025 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Shashank G <shashankgirish07@gmail.com>
 *          Navaneet Y V N <navaneetyvn.work@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

#include "qkd-key-management-layer.h"

#include "qkd-key-manager.h"

#include "ns3/log.h"

#include <mutex>

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("QkdKeyManagementLayer");
NS_OBJECT_ENSURE_REGISTERED(QkdKeyManagementLayer);

QkdKeyManagementLayer::QkdKeyManagementLayer(Ptr<QkdDevice> qkdDevice, uint16_t bufferSize)
    : m_qkdDevice(qkdDevice),
      m_bufferSize(bufferSize)
{
    NS_LOG_FUNCTION(this);
    m_qkdDevice->SetKeyGenerationCallback(
        MakeCallback(&QkdKeyManagementLayer::HandleKeyGenerationCallback, this));
    m_keyBuffer = std::vector<std::pair<std::string, std::pair<Address, KeyMetadata>>>();
}

QkdKeyManagementLayer::QkdKeyManagementLayer(const QkdKeyManagementLayer& o)
    : m_qkdDevice(o.m_qkdDevice),
      m_bufferSize(o.m_bufferSize)
{
    NS_LOG_FUNCTION(this);
    m_qkdDevice->SetKeyGenerationCallback(
        MakeCallback(&QkdKeyManagementLayer::HandleKeyGenerationCallback, this));
    m_keyBuffer = o.m_keyBuffer;
}

QkdKeyManagementLayer::~QkdKeyManagementLayer()
{
    NS_LOG_FUNCTION(this);
}

TypeId
QkdKeyManagementLayer::GetTypeId(void)
{
    static TypeId tid =
        TypeId("ns3::QkdKeyManagementLayer").SetParent<Object>().SetGroupName("Quantum");
    return tid;
}

void
QkdKeyManagementLayer::ResizeBuffer(uint16_t size)
{
    NS_LOG_FUNCTION(this << size);
    m_bufferSize = size;
    m_keyBuffer.resize(m_bufferSize);
}

void
QkdKeyManagementLayer::HandleKeyGenerationCallback(KeyGenerationData data)
{
    NS_LOG_FUNCTION(this << data.status);

    if (data.status == QkdProtocolStatus::SUCCESS)
    {
        NS_LOG_INFO("Key generation successful, key: " << data.key);
        // Key generation successful
        std::string key = data.key;
        KeyMetadata keyParams;
        keyParams.size = key.size();
        keyParams.creationTime = Simulator::Now();
        keyParams.expirationTime = Simulator::Now() + Seconds(3600);

        AddKeyToBuffer(key, m_qkdDevice->GetChannel()->GetDevice(1)->GetAddress(), keyParams);
    }
    else
    {
        NS_LOG_ERROR("Key generation failed with status: " << data.status);
    }
}

Ptr<QkdDevice>
QkdKeyManagementLayer::GetQkdDevice() const
{
    return m_qkdDevice;
}

void
QkdKeyManagementLayer::SetQkdDevice(Ptr<QkdDevice> qkdDevice)
{
    m_qkdDevice = qkdDevice;
}

void
QkdKeyManagementLayer::AddKeyToBuffer(std::string key,
                                      const Address& destination,
                                      KeyMetadata keyParams)
{
    NS_LOG_FUNCTION(this << key << destination << keyParams.size << Simulator::Now().GetMinutes());
    std::pair<Address, KeyMetadata> keyPair(destination, keyParams);
    if (m_keyBuffer.size() < m_bufferSize)
    {
        m_keyBuffer.push_back(std::make_pair(key, keyPair));
        NS_LOG_INFO("Key added to buffer: " << key << " for destination: " << destination);
        NS_LOG_INFO("Current buffer size: " << m_keyBuffer.size() << " Max size: " << m_bufferSize);
    }
    else
    {
        NS_LOG_ERROR("Key buffer is full, cannot add new key.");
    }
}

std::pair<std::string, std::pair<Address, KeyMetadata>>
QkdKeyManagementLayer::GetKeyFromBuffer()
{
    NS_LOG_FUNCTION(this);
    for (const auto& keyEntry : m_keyBuffer)
    {
        if (keyEntry.second.first == m_qkdDevice->GetChannel()->GetDevice(1)->GetAddress())
        {
            NS_LOG_INFO("Key found in buffer: " << keyEntry.first
                                                << " for destination: " << keyEntry.second.first);
            return keyEntry;
        }
    }
    NS_LOG_WARN("Key not found in buffer for destination: "
                << m_qkdDevice->GetChannel()->GetDevice(1)->GetAddress());
    return std::make_pair("", std::make_pair(Address(), KeyMetadata()));
}

uint16_t
QkdKeyManagementLayer::GetBufferMaxSize() const
{
    return m_bufferSize;
}

uint16_t
QkdKeyManagementLayer::GetCurrentBufferSize() const
{
    return m_keyBuffer.size();
}

void
QkdKeyManagementLayer::ScheduleKeyGeneration()
{
    NS_LOG_FUNCTION(this);

    Ptr<NetDevice> netDev = m_qkdDevice->GetChannel()->GetDevice(1);
    Ptr<QkdDevice> peerDevice = DynamicCast<QkdDevice>(netDev);

    Simulator::ScheduleNow(&QkdDevice::InitiateKeyGeneration, m_qkdDevice, 256, peerDevice);
}

void
QkdKeyManagementLayer::WaitForKeyGeneration(
    const Address& address,
    Callback<void, std::pair<std::string, std::pair<Address, KeyMetadata>>> cb,
    Time retryInterval)
{
    NS_LOG_FUNCTION(this << address);

    auto keyEntry = GetKeyFromBuffer();
    if (!keyEntry.first.empty())
    {
        cb(keyEntry); // Key is available, return immediately
    }
    else
    {
        // Retry after retryInterval
        Simulator::Schedule(retryInterval,
                            &QkdKeyManagementLayer::WaitForKeyGeneration,
                            this,
                            address,
                            cb,
                            retryInterval);
    }
}

} // namespace ns3
