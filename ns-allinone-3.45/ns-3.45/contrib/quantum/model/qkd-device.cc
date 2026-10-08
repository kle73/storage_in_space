/*
 * Copyright (c) 2025 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Aniket Singh <aniketsingh84646@gmail.com>
 *          Satyam Shukla <shuklasatyam774@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

#include "qkd-device.h"

#include "quantum-channel.h"

#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/simulator.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("QkdDevice");
NS_OBJECT_ENSURE_REGISTERED(QkdDevice);

TypeId
QkdDevice::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::QkdDevice")
                            .SetParent<QuantumDevice>()
                            .SetGroupName("QKD")
                            .AddConstructor<QkdDevice>();
    return tid;
}

QkdDevice::QkdDevice()
    : m_eave(false)
{
    NS_LOG_FUNCTION(this);
    SetReceiveCallback(MakeCallback(&QkdDevice::ReceiveFromClassicalChannel, this));
    m_randomVariableStream = CreateObject<UniformRandomVariable>();
}

QkdDevice::~QkdDevice()
{
    NS_LOG_FUNCTION(this);
}

void
QkdDevice::DoInitialize()
{
    NS_LOG_FUNCTION(this);

    this->SetReceiveCallback(MakeCallback(&QkdDevice::ReceiveFromClassicalChannel, this));
}

bool
QkdDevice::GetEave() const
{
    return m_eave;
}

void
QkdDevice::SetEave(bool eave)
{
    m_eave = eave;
}

void
QkdDevice::SetKeyGenerationCallback(Callback<void, KeyGenerationData> keyGenerationCallback)
{
    NS_LOG_FUNCTION(this);

    this->m_notifyKeyGenerationCallback = keyGenerationCallback;
}

void
QkdDevice::SendThroughQuantumChannel(Ptr<QkdDevice> senderDevice, Ptr<QBit> qbit)
{
    Simulator::Schedule(TimeStep(1),
                        &QkdDevice::SendQubit,
                        senderDevice,
                        qbit,
                        Mac48Address::ConvertFrom(senderDevice->SimpleNetDevice::GetAddress()));
}

void
QkdDevice::SendThroughClassicalChannel(Ptr<QkdDevice> senderDevice,
                                       Ptr<KeyExchangeInfo> keyExchangeInfo,
                                       Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(packet);
    bool sent = senderDevice->Send(packet, keyExchangeInfo->destinationDeviceAddress, 0);
    NS_LOG_INFO("Classical packet send status : " << sent);
}

void
QkdDevice::SetupKeyExchangeInfo(Mac48Address destinationDeviceAddress)
{
    Ptr<KeyExchangeInfo> keyExchangeInfo = CreateObject<KeyExchangeInfo>();
    keyExchangeInfo->destinationDeviceAddress = destinationDeviceAddress;

    Ptr<QkdProtocol> protocol = CreateObject<B92QkdProtocol>(
        MakeBoundCallback(&QkdDevice::SendThroughQuantumChannel, Ptr(this)),
        MakeBoundCallback(&QkdDevice::SendThroughClassicalChannel, Ptr(this), keyExchangeInfo));
    protocol->SetKeyGenerationCallback(m_notifyKeyGenerationCallback);
    keyExchangeInfo->protocol = protocol;
    m_keyExchangeInfoMap[destinationDeviceAddress] = keyExchangeInfo;
}

void
QkdDevice::InitiateKeyGeneration(std::size_t size, Ptr<QkdDevice> recvDevice)
{
    NS_LOG_FUNCTION(this << size << recvDevice);
    Mac48Address destinationDeviceAddress =
        Mac48Address::ConvertFrom(recvDevice->SimpleNetDevice::GetAddress());
    if (m_keyExchangeInfoMap.find(destinationDeviceAddress) == m_keyExchangeInfoMap.end())
    {
        SetupKeyExchangeInfo(destinationDeviceAddress);
        recvDevice->SetupKeyExchangeInfo(
            Mac48Address::ConvertFrom(this->SimpleNetDevice::GetAddress()));
    }
    Ptr<KeyExchangeInfo> keyExchangeInfo = m_keyExchangeInfoMap[destinationDeviceAddress];
    keyExchangeInfo->protocol->InitiateKeyGeneration(size);
}

void
QkdDevice::SendQubit(Ptr<QBit> qbit, Mac48Address sourceDeviceAddress)
{
    NS_LOG_FUNCTION(this << sourceDeviceAddress);
    m_channel->Send(qbit, sourceDeviceAddress);
}

void
QkdDevice::ReceiveQubit(Ptr<QBit> qbit, Mac48Address sourceDeviceAddress)
{
    NS_LOG_FUNCTION(this << sourceDeviceAddress << m_eave);

    if (m_eave)
    {
        int randomBasis = m_randomVariableStream->GetInteger(0, 1);
        Bit measurement = m_helper.MeasureQBit(*qbit, randomBasis);
        NS_LOG_DEBUG("Eavesdropper measured as : " << measurement << " basis: " << randomBasis);
    }
    else
    {
        NS_LOG_INFO(m_keyExchangeInfoMap.size());
        m_keyExchangeInfoMap[sourceDeviceAddress]->protocol->RecvQBit(qbit);
    }
}

bool
QkdDevice::ReceiveFromClassicalChannel(Ptr<NetDevice> device,
                                       Ptr<const Packet> packet,
                                       uint16_t protocol,
                                       const Address& from)
{
    NS_LOG_FUNCTION(this << device << packet << protocol << from);

    Mac48Address fromAddr = Mac48Address::ConvertFrom(from);
    Ptr<Packet> mutablePacket = Create<Packet>(*packet);
    m_keyExchangeInfoMap[fromAddr]->protocol->RecvClassical(mutablePacket);
    NS_LOG_INFO("Classical packet received");
    return 1;
}

} // namespace ns3
