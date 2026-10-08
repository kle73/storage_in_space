/*
 * Copyright (c) 2024 Tokushima University, Japan
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author:
 *
 *  Alberto Gallegos Ramonet <alramonet@is.tokushima-u.ac.jp>
 */

/**
 *
 *
 * Usage:
 *
 * Start a device as coordinator:
 *
 * ./ns3 run "uart-zigbee-exp --coordDev=true"
 *
 * Start a device as router after 30 seconds, assign the
 * dev id of 1 and transmit data with this id.
 *
 * ./ns3 run "uart-zigbee-exp --initTime=30 --devId=1"
 */

#include "ns3/constant-position-mobility-model.h"
#include "ns3/core-module.h"
#include "ns3/log.h"
#include "ns3/lr-wpan-fields.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/network-module.h"
#include "ns3/packet.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/simulator.h"
#include "ns3/single-model-spectrum-channel.h"
#include "ns3/uart-lr-wpan-net-device.h"
#include "ns3/zigbee-module.h"

#include <iostream>

using namespace ns3;
using namespace ns3::lrwpan;
using namespace ns3::zigbee;
using namespace ns3::uart;

NS_LOG_COMPONENT_DEFINE("ZigbeeAssociationJoin");

static void
NwkDataIndication(Ptr<ZigbeeStack> stack, NldeDataIndicationParams params, Ptr<Packet> p)
{
    std::vector<uint8_t> buffer;
    buffer.resize(p->GetSize());
    p->CopyData(buffer.data(), p->GetSize());
    std::string data(buffer.begin(), buffer.end());

    std::cout << "Received packet | " << data << " | Size: " << p->GetSize() << "\n";
}

static void
NwkNetworkFormationConfirm(Ptr<ZigbeeStack> stack, NlmeNetworkFormationConfirmParams params)
{
    std::cout << "NlmeNetworkFormationConfirmStatus = " << params.m_status << "\n";
}

static void
NwkNetworkDiscoveryConfirm(Ptr<ZigbeeStack> stack, NlmeNetworkDiscoveryConfirmParams params)
{
    // See Zigbee Specification r22.1.0, 3.6.1.4.1
    // This method implements a simplistic version of the method implemented
    // in a zigbee APL layer. In this layer a candidate Extended PAN Id must
    // be selected and a NLME-JOIN.request must be issued.

    if (params.m_status == NwkStatus::SUCCESS)
    {
        std::cout << " Network discovery confirm Received. Networks found:\n";

        for (const auto& netDescriptor : params.m_netDescList)
        {
            std::cout << " ExtPanID: 0x" << std::hex << netDescriptor.m_extPanId << std::dec
                      << " CH:  " << static_cast<uint32_t>(netDescriptor.m_logCh) << std::hex
                      << " Pan Id: 0x" << netDescriptor.m_panId << " stackprofile " << std::dec
                      << static_cast<uint32_t>(netDescriptor.m_stackProfile) << "\n";
        }

        NlmeJoinRequestParams joinParams;

        zigbee::CapabilityInformation capaInfo;
        capaInfo.SetDeviceType(ROUTER);
        capaInfo.SetAllocateAddrOn(true);

        joinParams.m_rejoinNetwork = zigbee::JoiningMethod::ASSOCIATION;
        joinParams.m_capabilityInfo = capaInfo.GetCapability();
        joinParams.m_extendedPanId = params.m_netDescList[0].m_extPanId;

        Simulator::ScheduleNow(&ZigbeeNwk::NlmeJoinRequest, stack->GetNwk(), joinParams);
    }
    else
    {
        NS_ABORT_MSG("Unable to discover networks | status: " << params.m_status);
    }
}

static void
NwkJoinConfirm(Ptr<ZigbeeStack> stack, NlmeJoinConfirmParams params)
{
    if (params.m_status == NwkStatus::SUCCESS)
    {
        std::cout << Simulator::Now().As(Time::S)
                  << " The device joined the network SUCCESSFULLY with short address " << std::hex
                  << params.m_networkAddress << " on the Extended PAN Id: " << std::hex
                  << params.m_extendedPanId << "\n"
                  << std::dec;

        // 3 - After dev 1 is associated, it should be started as a router
        //     (i.e. it becomes able to accept request from other devices to join the network)
        NlmeStartRouterRequestParams startRouterParams;
        Simulator::ScheduleNow(&ZigbeeNwk::NlmeStartRouterRequest,
                               stack->GetNwk(),
                               startRouterParams);
    }
    else
    {
        std::cout << " The device FAILED to join the network with status " << params.m_status
                  << "\n";
    }
}

int
main(int argc, char* argv[])
{
    bool coordDev = false;
    uint32_t initTime = 2;
    uint32_t devId = 0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("coordDev", "Indication if dev is the coord", coordDev);
    cmd.AddValue("initTime", "Initialization delay time in secs", initTime);
    cmd.AddValue("devId", "Device identifier", devId);
    cmd.Parse(argc, argv);

    LogComponentEnableAll(LogLevel(LOG_PREFIX_TIME | LOG_PREFIX_FUNC));
    LogComponentEnable("ZigbeeNwk", LOG_LEVEL_DEBUG);
    LogComponentEnable("ZigbeeNwkTables", LOG_LEVEL_DEBUG);
    // LogComponentEnable("ZigbeeStack", LOG_LEVEL_DEBUG);
    // LogComponentEnable("UartLrWpanMac", LOG_LEVEL_DEBUG);
    // LogComponentEnable("UartLrWpanNetDevice", LOG_LEVEL_DEBUG);

    // We are using a real piece of hardware, therefore we need to use realtime
    GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::RealtimeSimulatorImpl"));
    // Enable calculation of FCS in the trailers. Only necessary when interacting with real devices
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));

    //// Set UART NetDevice

    // Coordinator
    Ptr<Node> node = CreateObject<Node>();
    Ptr<UartLrWpanNetDevice> uartNetDevice = CreateObject<UartLrWpanNetDevice>("/dev/ttyUSB0");
    node->AddDevice(uartNetDevice);

    // Configure NWK
    NetDeviceContainer uartDevices;
    uartDevices.Add(uartNetDevice);

    ZigbeeHelper zigbee;
    ZigbeeStackContainer zigbeeStackContainer = zigbee.Install(uartDevices);

    Ptr<ZigbeeStack> zstack0 = zigbeeStackContainer.Get(0);

    // NWK callbacks hooks

    zstack0->GetNwk()->SetNlmeNetworkFormationConfirmCallback(
        MakeBoundCallback(&NwkNetworkFormationConfirm, zstack0));

    zstack0->GetNwk()->SetNldeDataIndicationCallback(
        MakeBoundCallback(&NwkDataIndication, zstack0));

    zstack0->GetNwk()->SetNlmeNetworkDiscoveryConfirmCallback(
        MakeBoundCallback(&NwkNetworkDiscoveryConfirm, zstack0));

    zstack0->GetNwk()->SetNlmeJoinConfirmCallback(MakeBoundCallback(&NwkJoinConfirm, zstack0));

    if (coordDev)
    {
        // 1 - Initiate the Zigbee coordinator, start the network
        NlmeNetworkFormationRequestParams netFormParams;
        netFormParams.m_scanChannelList.channelPageCount = 1;
        netFormParams.m_scanChannelList.channelsField[0] = 0x00000800; // Ch. 11
        netFormParams.m_scanDuration = 0;
        netFormParams.m_superFrameOrder = 15;
        netFormParams.m_beaconOrder = 15;

        Simulator::ScheduleWithContext(zstack0->GetNode()->GetId(),
                                       Seconds(initTime),
                                       &ZigbeeNwk::NlmeNetworkFormationRequest,
                                       zstack0->GetNwk(),
                                       netFormParams);
    }
    else
    {
        // 2- Let the dev1 (Router) discovery the coordinator and join the network, after
        //    this, it will become router itself(call to NLME-START-ROUTER.request).
        NlmeNetworkDiscoveryRequestParams netDiscParams;
        netDiscParams.m_scanChannelList.channelPageCount = 1;
        netDiscParams.m_scanChannelList.channelsField[0] = 0x00000800; // Ch. 11
        netDiscParams.m_scanDuration = 7;
        Simulator::ScheduleWithContext(zstack0->GetNode()->GetId(),
                                       Seconds(initTime),
                                       &ZigbeeNwk::NlmeNetworkDiscoveryRequest,
                                       zstack0->GetNwk(),
                                       netDiscParams);

        std::ostringstream msg;
        msg << "Hello ns-3 from dev " << devId << '\0';
        Ptr<Packet> p = Create<Packet>((uint8_t*)msg.str().c_str(), msg.str().length());

        NldeDataRequestParams dataReqParams;
        dataReqParams.m_dstAddrMode = UCST_BCST;
        dataReqParams.m_dstAddr = Mac16Address("00:00");
        dataReqParams.m_nsduHandle = 1;
        dataReqParams.m_discoverRoute = ENABLE_ROUTE_DISCOVERY;
        Simulator::ScheduleWithContext(zstack0->GetNode()->GetId(),
                                       Seconds(initTime + 10),
                                       &ZigbeeNwk::NldeDataRequest,
                                       zstack0->GetNwk(),
                                       dataReqParams,
                                       p);
    }

    Simulator::Stop(Seconds(3600));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
