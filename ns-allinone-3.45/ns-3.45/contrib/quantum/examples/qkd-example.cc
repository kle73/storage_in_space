/*
 * Copyright (c) 2025 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Anirudh V Gubbi <anirudhvgubbi@gmail.com>
 *          Akash Ravi <akashravi28055@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

#include "ns3/core-module.h"
#include "ns3/log.h"
#include "ns3/network-module.h"
#include "ns3/node-container.h"
#include "ns3/qkd-device.h"
#include "ns3/qkd-protocol.h"
#include "ns3/quantum-channel.h"
#include "ns3/simple-channel.h"
#include "ns3/simple-net-device.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("QkdExample");

void
NotifyKeyGenerated(KeyGenerationData data)
{
    NS_LOG_INFO("Key generation status: " << data.status);
    if (data.status == QkdProtocolStatus::SUCCESS)
    {
        NS_LOG_INFO("Key generated: " << data.key);
    }
}

int
main(int argc, char* argv[])
{
    // Enable logging
    LogComponentEnable("QkdExample", LOG_LEVEL_ALL);
    LogComponentEnable("QkdDevice", LOG_LEVEL_ALL);      // Optional
    LogComponentEnable("QkdProtocol", LOG_LEVEL_ALL);    // Optional
    LogComponentEnable("QuantumChannel", LOG_LEVEL_ALL); // Optional

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Create channels
    Ptr<SimpleChannel> classicalChannel = CreateObject<SimpleChannel>();
    Ptr<QuantumChannel> quantumChannel = CreateObject<QuantumChannel>();

    // Create and configure QkdDevices
    Ptr<QkdDevice> devA = CreateObject<QkdDevice>();
    Ptr<QkdDevice> devB = CreateObject<QkdDevice>();

    Mac48Address addrA("00:00:00:00:00:01");
    Mac48Address addrB("00:00:00:00:00:02");

    devA->SetAddress(addrA);
    devB->SetAddress(addrB);

    devA->SetNode(nodes.Get(0));
    devB->SetNode(nodes.Get(1));

    devA->SetChannel(classicalChannel);
    devB->SetChannel(classicalChannel);

    devA->SetQuantumChannel(quantumChannel);
    devB->SetQuantumChannel(quantumChannel);

    // Add devices to the nodes
    nodes.Get(0)->AddDevice(devA);
    nodes.Get(1)->AddDevice(devB);

    // Add trusted devices to quantum channel
    quantumChannel->AddDevice(addrA, devA, Seconds(1.0)); // Optional delay
    quantumChannel->AddDevice(addrB, devB, Seconds(1.5)); // Optional delay

    // Start key generation
    Simulator::Schedule(Seconds(2.0), &QkdDevice::InitiateKeyGeneration, devA, 512, devB);

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
