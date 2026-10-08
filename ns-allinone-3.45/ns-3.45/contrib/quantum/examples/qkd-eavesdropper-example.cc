/*
 * Copyright (c) 2025 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Satyam Shukla <shuklasatyam774@gmail.com>
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

    NodeContainer nodes;
    nodes.Create(3); // Increased to 3 to include eavesdropper

    // Create channels
    Ptr<SimpleChannel> classicalChannel = CreateObject<SimpleChannel>();
    Ptr<QuantumChannel> quantumChannel = CreateObject<QuantumChannel>();

    // Create and configure QkdDevices for Alice and Bob
    Ptr<QkdDevice> devA = CreateObject<QkdDevice>();
    Ptr<QkdDevice> devB = CreateObject<QkdDevice>();
    Ptr<QkdDevice> devEve = CreateObject<QkdDevice>(); // Eavesdropper

    Mac48Address addrA("00:00:00:00:00:01");
    Mac48Address addrB("00:00:00:00:00:02");
    Mac48Address addrE("00:00:00:00:00:03");

    devA->SetAddress(addrA);
    devB->SetAddress(addrB);
    devEve->SetAddress(addrE);

    // Set nodes
    devA->SetNode(nodes.Get(0));
    devB->SetNode(nodes.Get(1));
    devEve->SetNode(nodes.Get(2));

    // Set channels
    devA->SetChannel(classicalChannel);
    devB->SetChannel(classicalChannel);
    // Eavesdropper does not participate in classical communication

    devA->SetQuantumChannel(quantumChannel);
    devB->SetQuantumChannel(quantumChannel);
    devEve->SetQuantumChannel(quantumChannel);

    // Mark Eve as an eavesdropper
    devEve->SetEave(true);

    // Add devices to the nodes
    nodes.Get(0)->AddDevice(devA);
    nodes.Get(1)->AddDevice(devB);
    nodes.Get(2)->AddDevice(devEve);

    // Add all devices to the quantum channel
    quantumChannel->AddDevice(addrA, devA, Seconds(1.0));
    quantumChannel->AddDevice(addrB, devB, Seconds(2));
    quantumChannel->AddEavesdropper(addrE, devEve, Seconds(1)); // Eavesdropper inserted into path

    // Set key generation callback
    devA->SetKeyGenerationCallback(MakeCallback(&NotifyKeyGenerated));
    devB->SetKeyGenerationCallback(MakeCallback(&NotifyKeyGenerated));

    // Start key generation from Alice to Bob
    Simulator::Schedule(Seconds(2.0), &QkdDevice::InitiateKeyGeneration, devA, 1024, devB);

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
