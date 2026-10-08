/*
 * Copyright (c) 2024 Tokushima University, Japan.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author:
 *  Alberto Gallegos Ramonet <alramonet@is.tokushima-u.ac.jp>
 */

#include "ns3/core-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/internet-module.h"
// #include "ns3/lr-wpan-module.h"
// #include "ns3/mobility-module.h"
// #include "ns3/propagation-module.h"
#include "ns3/sixlowpan-module.h"
// #include "ns3/spectrum-module.h"
#include "ns3/lr-wpan-fields.h"
#include "ns3/uart-lr-wpan-net-device.h"

#include <fstream>

using namespace ns3;
using namespace ns3::lrwpan;
using namespace ns3::uart;

int
main(int argc, char** argv)
{
    bool verbose = false;
    bool disablePcap = false;
    bool disableAsciiTrace = false;
    bool enableLSixlowLogLevelInfo = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "turn on log components", verbose);
    cmd.AddValue("disable-pcap", "disable PCAP generation", disablePcap);
    cmd.AddValue("disable-asciitrace", "disable ascii trace generation", disableAsciiTrace);
    cmd.AddValue("enable-sixlowpan-loginfo",
                 "enable sixlowpan LOG_LEVEL_INFO (used for tests)",
                 enableLSixlowLogLevelInfo);
    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnable("Ping", LOG_LEVEL_ALL);
        LogComponentEnable("LrWpanMac", LOG_LEVEL_ALL);
        LogComponentEnable("LrWpanPhy", LOG_LEVEL_ALL);
        LogComponentEnable("LrWpanNetDevice", LOG_LEVEL_ALL);
        LogComponentEnable("SixLowPanNetDevice", LOG_LEVEL_ALL);
    }

    // We are using a real piece of hardware, therefore we need to use realtime
    GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::RealtimeSimulatorImpl"));
    // Enable calculation of FCS in the trailers. Only necessary when interacting with real devices
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));

    // Create 1 PAN coordinator node, and 1 end device
    Ptr<Node> coord1 = CreateObject<Node>();
    Ptr<UartLrWpanNetDevice> coord1NetDevice = CreateObject<UartLrWpanNetDevice>("/dev/ttyUSB0");
    coord1->AddDevice(coord1NetDevice);

    Ptr<Node> endNode = CreateObject<Node>();
    Ptr<UartLrWpanNetDevice> endNodeNetDevice = CreateObject<UartLrWpanNetDevice>("/dev/ttyUSB1");
    endNode->AddDevice(endNodeNetDevice);

    NodeContainer nodes;
    nodes.Add(coord1);
    nodes.Add(endNode);

    NetDeviceContainer uartDevices;
    uartDevices.Add(coord1NetDevice);
    uartDevices.Add(endNodeNetDevice);

    /* NodeContainer nodes;
     nodes.Create(2);

     // Mobility is only representative as it has no effect when using
     // real devices
     MobilityHelper mobility;
     mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
     mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                   "MinX",
                                   DoubleValue(0.0),
                                   "MinY",
                                   DoubleValue(0.0),
                                   "DeltaX",
                                   DoubleValue(20),
                                   "DeltaY",
                                   DoubleValue(20),
                                   "GridWidth",
                                   UintegerValue(3),
                                   "LayoutType",
                                   StringValue("RowFirst"));
     mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
     mobility.Install(nodes);

     LrWpanHelper lrWpanHelper;
     lrWpanHelper.SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");
     lrWpanHelper.AddPropagationLossModel("ns3::LogDistancePropagationLossModel");
     // Add and install the LrWpanNetDevice for each node
     NetDeviceContainer lrwpanDevices = lrWpanHelper.Install(nodes);

     // Manual PAN association and extended and short address assignment.
     // Association using the MAC functions can also be used instead of this step.
     lrWpanHelper.CreateAssociatedPan(lrwpanDevices, 1);*/

    // Manually set the PANID and the short address of the devices to
    // enable the communication in lr-wpan devices.
    Ptr<MacPibAttributes> pibAttr1 = Create<MacPibAttributes>();
    pibAttr1->macShortAddress = Mac16Address("00:01");
    Simulator::ScheduleWithContext(coord1NetDevice->GetNode()->GetId(),
                                   Seconds(0.5),
                                   &UartLrWpanMac::MlmeSetRequest,
                                   coord1NetDevice->GetMac(),
                                   MacPibAttributeIdentifier::macShortAddress,
                                   pibAttr1);

    Ptr<MacPibAttributes> pibAttr2 = Create<MacPibAttributes>();
    pibAttr2->macShortAddress = Mac16Address("00:02");
    Simulator::ScheduleWithContext(endNodeNetDevice->GetNode()->GetId(),
                                   Seconds(0.8),
                                   &UartLrWpanMac::MlmeSetRequest,
                                   endNodeNetDevice->GetMac(),
                                   MacPibAttributeIdentifier::macShortAddress,
                                   pibAttr2);

    Ptr<MacPibAttributes> pibAttr3 = Create<MacPibAttributes>();
    pibAttr3->macPanId = 0xCAFE;
    Simulator::ScheduleWithContext(coord1NetDevice->GetNode()->GetId(),
                                   Seconds(1.5),
                                   &UartLrWpanMac::MlmeSetRequest,
                                   coord1NetDevice->GetMac(),
                                   MacPibAttributeIdentifier::macPanId,
                                   pibAttr3);

    Ptr<MacPibAttributes> pibAttr4 = Create<MacPibAttributes>();
    pibAttr4->macPanId = 0xCAFE;
    Simulator::ScheduleWithContext(endNodeNetDevice->GetNode()->GetId(),
                                   Seconds(1.8),
                                   &UartLrWpanMac::MlmeSetRequest,
                                   endNodeNetDevice->GetMac(),
                                   MacPibAttributeIdentifier::macPanId,
                                   pibAttr4);

    InternetStackHelper internetv6;
    internetv6.Install(nodes);

    SixLowPanHelper sixlowpan;
    NetDeviceContainer devices = sixlowpan.Install(uartDevices);

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer deviceInterfaces;
    deviceInterfaces = ipv6.Assign(devices);

    /* if (enableLSixlowLogLevelInfo)
     {
         std::cout << "Device 0: pseudo-Mac-48 "
                   << Mac48Address::ConvertFrom(devices.Get(0)->GetAddress()) << ", IPv6 Address "
                   << deviceInterfaces.GetAddress(0, 1) << std::endl;
         std::cout << "Device 1: pseudo-Mac-48 "
                   << Mac48Address::ConvertFrom(devices.Get(1)->GetAddress()) << ", IPv6 Address "
                   << deviceInterfaces.GetAddress(1, 1) << std::endl;
     }*/

    /* uint32_t packetSize = 16;
     uint32_t maxPacketCount = 5;
     Time interPacketInterval = Seconds(1.);
     PingHelper ping(deviceInterfaces.GetAddress(1, 1));

     ping.SetAttribute("Count", UintegerValue(maxPacketCount));
     ping.SetAttribute("Interval", TimeValue(interPacketInterval));
     ping.SetAttribute("Size", UintegerValue(packetSize));
     ApplicationContainer apps = ping.Install(nodes.Get(0));

     apps.Start(Seconds(2.0));
     apps.Stop(Seconds(20.0));
     */

    /*if (!disableAsciiTrace)
    {
        AsciiTraceHelper ascii;
        lrWpanHelper.EnableAsciiAll(ascii.CreateFileStream("Ping-6LoW-lr-wpan.tr"));
    }
    if (!disablePcap)
    {
        lrWpanHelper.EnablePcapAll(std::string("Ping-6LoW-lr-wpan"), true);
    }*/
    if (enableLSixlowLogLevelInfo)
    {
        Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>(&std::cout);
        Ipv6RoutingHelper::PrintNeighborCacheAllAt(Seconds(9), routingStream);
    }

    Simulator::Stop(Seconds(20));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
