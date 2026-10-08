/**
 * Copyright (c) 2025 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Shashank G <shashankgirish07@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/quantum-module.h"

using namespace ns3;

int
main(int argc, char* argv[])
{
    bool verbose = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "Tell application to log if true", verbose);
    cmd.Parse(argc, argv);

    if (verbose)
    {
        // LogComponentEnable("SymmetricEncryption", LOG_LEVEL_ALL);
        // LogComponentEnable("QkdDevice", LOG_LEVEL_INFO);
        // LogComponentEnable("QkdKeyManager", LOG_LEVEL_ALL);
        // LogComponentEnable("QkdKeyManagementLayer", LOG_LEVEL_ALL);
        // LogComponentEnable("QkdAppHeader", LOG_LEVEL_ALL);
        // LogComponentEnable("QkdAppTrailer", LOG_LEVEL_ALL);
        // LogComponentEnable("QkdKeyManagerApplication", LOG_LEVEL_ALL);
        LogComponentEnable("QkdSecureSender", LOG_LEVEL_ALL);
        LogComponentEnable("QkdSecureReceiver", LOG_LEVEL_ALL);
    }

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Create quantum and classical channels
    Ptr<SimpleChannel> classicalChannel = CreateObject<SimpleChannel>();
    Ptr<QuantumChannel> quantumChannel = CreateObject<QuantumChannel>();

    // Create QKD devices
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

    nodes.Get(0)->AddDevice(devA);
    nodes.Get(1)->AddDevice(devB);

    quantumChannel->AddDevice(addrA, devA, Seconds(1.0));
    quantumChannel->AddDevice(addrB, devB, Seconds(1.5));

    // Classical (TCP/IP) network setup
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer netDevices = p2p.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces = ipv4.Assign(netDevices);

    // Setup QKD Key Manager Applications
    // Assume devA and devB are QkdDevice pointers
    Ipv4Address ipA = interfaces.GetAddress(0);
    Ipv4Address ipB = interfaces.GetAddress(1);

    std::cout << "Node A: " << nodes.Get(0)->GetNDevices() << " devices, IP: " << ipA << std::endl;

    // 1. Setup Key Manager Applications with helper
    std::vector<Address> peerA = {InetSocketAddress(ipB, 443)};
    std::vector<Address> peerB = {InetSocketAddress(ipA, 443)};

    QkdKeyManagerApplicationHelper kmHelperA(ipA, peerA, {devA});
    QkdKeyManagerApplicationHelper kmHelperB(ipB, peerB, {devB});

    ApplicationContainer kmAppsA = kmHelperA.Install(nodes.Get(0));
    ApplicationContainer kmAppsB = kmHelperB.Install(nodes.Get(1));

    kmAppsA.Start(Seconds(1.0));
    kmAppsA.Stop(Minutes(20.0));
    kmAppsB.Start(Seconds(1.0));
    kmAppsB.Stop(Minutes(20.0));

    std::cout << "Key Manager Applications installed on nodes." << std::endl;

    // 2. Setup QKD Secure Sender and Receiver
    QkdSecureSenderHelper senderHelper(InetSocketAddress(ipB, 80),
                                       ipA,
                                       InetSocketAddress(ipA, 443));
    QkdSecureReceiverHelper receiverHelper(ipB, InetSocketAddress(ipB, 443));

    ApplicationContainer senderApp = senderHelper.Install(nodes.Get(0));
    ApplicationContainer receiverApp = receiverHelper.Install(nodes.Get(1));

    senderApp.Start(Seconds(1.0));
    senderApp.Stop(Minutes(20.0));
    receiverApp.Start(Seconds(1.0));
    receiverApp.Stop(Minutes(20.0));

    Simulator::Run();
    Simulator::Stop(Minutes(20.0));
    Simulator::Destroy();
    return 0;
}
