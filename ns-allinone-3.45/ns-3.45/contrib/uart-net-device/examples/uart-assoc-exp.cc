/*
 * Copyright (c) 2024 Tokushima University, Japan.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author:
 *  Alberto Gallegos Ramonet <alramonet@is.tokushima-u.ac.jp>
 */

#include "ns3/core-module.h"
#include "ns3/log.h"
#include "ns3/lr-wpan-fields.h"
#include "ns3/network-module.h"
#include "ns3/packet.h"
#include "ns3/random-variable-stream.h"
#include "ns3/simulator.h"
#include "ns3/uart-lr-wpan-helper.h"
#include "ns3/uart-lr-wpan-net-device.h"

#include <iostream>

using namespace ns3;
using namespace ns3::lrwpan;
using namespace ns3::uart;

static void
ScanConfirm(Ptr<UartLrWpanNetDevice> device, MlmeScanConfirmParams params)
{
    std::cout << Simulator::Now().As(Time::S) << " Node " << device->GetNode()->GetId()
              << ", Scan confirm status: " << static_cast<uint32_t>(params.m_status)
              << " | Type: " << static_cast<uint32_t>(params.m_scanType) << "\n";
    if (params.m_scanType == MLMESCAN_ED)
    {
        std::cout << "Energy Detected(" << static_cast<uint32_t>(params.m_resultListSize) << "):\n";
        for (const auto& energy : params.m_energyDetList)
        {
            std::cout << static_cast<uint32_t>(energy) << "\n";
        }
    }
    else
    {
        std::cout << " Networks Found (" << static_cast<uint32_t>(params.m_resultListSize)
                  << "):\n";

        for (const auto& descriptor : params.m_panDescList)
        {
            std::cout << "   Coord. Address Mode: " << descriptor.m_coorAddrMode << "\n";
            if (descriptor.m_coorAddrMode == SHORT_ADDR)
            {
                std::cout << "   Coord Address: " << descriptor.m_coorShortAddr << "\n";
            }
            else if (descriptor.m_coorAddrMode == EXT_ADDR)
            {
                std::cout << "   Coord Address: " << descriptor.m_coorExtAddr << "\n";
            }
            std::cout << "   Pan Id: 0x" << std::hex << descriptor.m_coorPanId << std::dec << "\n";
            std::cout << "   LQI: " << static_cast<uint32_t>(descriptor.m_linkQuality) << "\n";
            std::cout << "   Channel: " << static_cast<uint32_t>(descriptor.m_logCh) << "\n";
            std::cout << "   Page: " << static_cast<uint32_t>(descriptor.m_logChPage) << "\n";
            std::cout << "   GTS permit: " << descriptor.m_gtsPermit << "\n";
            std::cout << "   Superframe Spec: \n";
            auto superframe = SuperframeField(descriptor.m_superframeSpec);
            std::cout << "      Beacon Order: "
                      << static_cast<uint32_t>(superframe.GetBeaconOrder()) << "\n";
            std::cout << "      Superframe Order: "
                      << static_cast<uint32_t>(superframe.GetFrameOrder()) << "\n";
            std::cout << "      Associate Permit: " << superframe.IsAssocPermit() << "\n";
            std::cout << "      Pan Coord: " << superframe.IsPanCoor() << "\n";
            std::cout << "      Battery Life Ext: " << superframe.IsBattLifeExt() << "\n";
        }

        // Use the scan results to initiate association with the first coordinator found.
        MlmeAssociateRequestParams associateParams;
        associateParams.m_chNum = params.m_panDescList[0].m_logCh;
        associateParams.m_chPage = params.m_panDescList[0].m_logChPage;
        associateParams.m_coordAddrMode = params.m_panDescList[0].m_coorAddrMode;
        associateParams.m_coordPanId = params.m_panDescList[0].m_coorPanId;
        associateParams.m_capabilityInfo = 0x80; // Assign short address
        associateParams.m_coordShortAddr = params.m_panDescList[0].m_coorShortAddr;

        device->GetMac()->MlmeAssociateRequest(associateParams);
    }
}

static void
AssociateIndication(Ptr<UartLrWpanNetDevice> device, MlmeAssociateIndicationParams params)
{
    std::cout << Simulator::Now().As(Time::S) << " Node " << device->GetNode()->GetId()
              << ", AssociateIndication: Coord Received Assoc. Req. from " << params.m_extDevAddr
              << "\n";

    MlmeAssociateResponseParams respParams;

    CapabilityField capability;
    capability.SetCapability(params.capabilityInfo);
    // Allocate a random address and avoid special addresses.

    // Addresses over 0x7FFF have special meanings, do not use them.
    // The range 0x8000 to 0x9FFF is used for multicast in other networks
    // (i.e. IPV6 over IEEE 802.15.4) for this reason, we avoid this range as well.
    Ptr<UniformRandomVariable> rndVar = CreateObject<UniformRandomVariable>();
    uint16_t rndValue = rndVar->GetInteger(1, 0x7FFF);
    uint16_t rndValue2 = rndVar->GetInteger(0xA000, 0xFFF7);
    uint16_t rndValue3 = rndVar->GetInteger(1, 2);

    if (rndValue3 == 1)
    {
        respParams.m_assocShortAddr = Mac16Address(rndValue);
    }
    else
    {
        respParams.m_assocShortAddr = Mac16Address(rndValue2);
    }

    respParams.m_extDevAddr = params.m_extDevAddr;
    respParams.m_status = MacStatus::SUCCESS;

    device->GetMac()->MlmeAssociateResponse(respParams);
}

static void
AssociateConfirm(Ptr<UartLrWpanNetDevice> device, MlmeAssociateConfirmParams params)
{
    std::cout << Simulator::Now().As(Time::S) << " Node " << device->GetNode()->GetId()
              << ", Associate Confirm: Status " << static_cast<uint32_t>(params.m_status)
              << "| Address: " << params.m_assocShortAddr << "\n";
}

static void
StartConfirm(Ptr<UartLrWpanNetDevice> device, MlmeStartConfirmParams params)
{
    std::cout << Simulator::Now().As(Time::S) << " Node " << device->GetNode()->GetId()
              << ", Start confirm | Status :" << static_cast<uint32_t>(params.m_status) << "\n";
}

static void
DataConfirm(Ptr<UartLrWpanNetDevice> device, McpsDataConfirmParams params)
{
    /*std::cout << Simulator::Now().As(Time::S) << " Node " << device->GetNode()->GetId()
              << ", Data confirm | Status :" << static_cast<uint32_t>(params.m_status)
              << " | Msdu handle " << static_cast<uint32_t>(params.m_msduHandle) << "\n";*/
}

static void
DataIndication(Ptr<UartLrWpanNetDevice> device, McpsDataIndicationParams params, Ptr<Packet> p)
{
    std::vector<uint8_t> buffer;
    buffer.resize(p->GetSize());
    p->CopyData(buffer.data(), p->GetSize());
    std::string data(buffer.begin(), buffer.end());

    std::cout << Simulator::Now().As(Time::S) << " Node " << device->GetNode()->GetId()
              << ", Data Indication :"
              << " SrcAddr: " << params.m_srcAddr << "|"
              << " DstAddr: " << params.m_dstAddr << "|"
              << " LQI: " << static_cast<uint32_t>(params.m_mpduLinkQuality) << "|"
              << " Msdu size: " << p->GetSize() << "|"
              << " Data: " << data << "\n";
}

static void
SetConfirm(Ptr<UartLrWpanNetDevice> device, MlmeSetConfirmParams params)
{
    std::cout << Simulator::Now().As(Time::S) << " Node " << device->GetNode()->GetId()
              << ", In SET Confirm with id attribute 0x" << std::hex << params.id << std::dec
              << " and status 0x" << std::hex << static_cast<uint32_t>(params.m_status) << std::dec
              << "\n";
}

static void
GetConfirm(Ptr<UartLrWpanNetDevice> device,
           MacStatus status,
           MacPibAttributeIdentifier id,
           Ptr<MacPibAttributes> pibattr)
{
    switch (id)
    {
    case MacPibAttributeIdentifier::pCurrentChannel: {
        std::cout << Simulator::Now().As(Time::S) << " Node " << device->GetNode()->GetId()
                  << ", In GET Confirm with id attribute " << id << " and status 0x" << std::hex
                  << static_cast<uint32_t>(status) << std::dec
                  << " phyCurrentChannel: " << static_cast<uint32_t>(pibattr->pCurrentChannel)
                  << "\n";
        break;
    }
    case MacPibAttributeIdentifier::pCurrentPage: {
        std::cout << Simulator::Now().As(Time::S) << " Node " << device->GetNode()->GetId()
                  << ", In GET Confirm with id attribute " << id << " and status 0x" << std::hex
                  << static_cast<uint32_t>(status) << std::dec
                  << " phyPage: " << static_cast<uint32_t>(pibattr->pCurrentPage) << "\n";
        break;
    }
    case MacPibAttributeIdentifier::macPanId:
        std::cout << Simulator::Now().As(Time::S) << " Node " << device->GetNode()->GetId()
                  << ", In GET Confirm with id attribute" << id << " and status 0x" << std::hex
                  << static_cast<uint32_t>(status) << std::dec << " macPanId 0x" << std::hex
                  << pibattr->macPanId << std::dec << "\n";
        break;
    case MacPibAttributeIdentifier::macShortAddress:
        std::cout << Simulator::Now().As(Time::S) << " Node " << device->GetNode()->GetId()
                  << ", In GET Confirm with id attribute " << id << " and status 0x" << std::hex
                  << static_cast<uint32_t>(status) << std::dec << " macShortAddress ["
                  << pibattr->macShortAddress << "]\n";
        break;
    case MacPibAttributeIdentifier::macExtendedAddress:
        std::cout << Simulator::Now().As(Time::S) << " Node " << device->GetNode()->GetId()
                  << ", In GET Confirm with id attribute " << id << " and status 0x" << std::hex
                  << static_cast<uint32_t>(status) << std::dec << " macExtendedAddress ["
                  << pibattr->macExtendedAddress << "]\n";
        break;
    case MacPibAttributeIdentifier::macBeaconPayloadLength:
        std::cout << Simulator::Now().As(Time::S) << " Node " << device->GetNode()->GetId()
                  << ", In GET Confirm with id attribute " << id << " and status 0x" << std::hex
                  << static_cast<uint32_t>(status) << std::dec << " macBeaconPayloadLength "
                  << static_cast<uint32_t>(pibattr->macBeaconPayloadLength) << "\n";
        break;
    case MacPibAttributeIdentifier::macBeaconPayload: {
        std::string data(pibattr->macBeaconPayload.begin(), pibattr->macBeaconPayload.end());
        std::cout << Simulator::Now().As(Time::S) << " Node " << device->GetNode()->GetId()
                  << ", In GET Confirm with id attribute " << id << " and status 0x" << std::hex
                  << static_cast<uint32_t>(status) << std::dec << " macBeaconPayload: " << data
                  << "\n";
        break;
    }
    default:
        std::cout << Simulator::Now().As(Time::S) << " Node " << device->GetNode()->GetId()
                  << " Attribute not listed.\n";
        break;
    }
}

int
main(int argc, char* argv[])
{
    uint8_t numNodes = 2;

    CommandLine cmd(__FILE__);
    cmd.AddValue("numNodes", "Number of nodes (minimum 2)", numNodes);
    cmd.Parse(argc, argv);

    LogComponentEnableAll(LogLevel(LOG_PREFIX_TIME | LOG_PREFIX_FUNC));
    LogComponentEnable("UartLrWpanHelper", LOG_LEVEL_DEBUG);

    // We are using a real piece of hardware, therefore we need to use realtime
    GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::RealtimeSimulatorImpl"));
    // Enable calculation of FCS in the trailers. Only necessary when interacting with real devices
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));

    NS_ASSERT_MSG(numNodes >= 2, "A minimum of 2 nodes is required");

    NodeContainer nodes;
    nodes.Create(numNodes);

    UartLrWpanHelper uartLrWpanHelper;
    uartLrWpanHelper.Install(nodes);

    for (auto i = nodes.Begin(); i != nodes.End(); i++)
    {
        Ptr<Node> node = *i;
        Ptr<NetDevice> netDevice = node->GetDevice(0);
        Ptr<UartLrWpanNetDevice> uartNetDevice = DynamicCast<UartLrWpanNetDevice>(netDevice);

        // Confirm and Indication primitives callback hooks used by
        // devices
        uartNetDevice->GetMac()->SetMlmeStartConfirmCallback(
            MakeBoundCallback(&StartConfirm, uartNetDevice));

        uartNetDevice->GetMac()->SetMcpsDataConfirmCallback(
            MakeBoundCallback(&DataConfirm, uartNetDevice));
        uartNetDevice->GetMac()->SetMcpsDataIndicationCallback(
            MakeBoundCallback(&DataIndication, uartNetDevice));

        uartNetDevice->GetMac()->SetMlmeSetConfirmCallback(
            MakeBoundCallback(&SetConfirm, uartNetDevice));
        uartNetDevice->GetMac()->SetMlmeGetConfirmCallback(
            MakeBoundCallback(&GetConfirm, uartNetDevice));

        uartNetDevice->GetMac()->SetMlmeScanConfirmCallback(
            MakeBoundCallback(&ScanConfirm, uartNetDevice));
        uartNetDevice->GetMac()->SetMlmeAssociateConfirmCallback(
            MakeBoundCallback(&AssociateConfirm, uartNetDevice));
        uartNetDevice->GetMac()->SetMlmeAssociateIndicationCallback(
            MakeBoundCallback(&AssociateIndication, uartNetDevice));

        if (node->GetId() == 0)
        {
            // Set the Coordinator short address
            Ptr<MacPibAttributes> pibAttr1 = Create<MacPibAttributes>();
            pibAttr1->macShortAddress = Mac16Address("00:00");
            Simulator::ScheduleWithContext(uartNetDevice->GetNode()->GetId(),
                                           Seconds(0),
                                           &UartLrWpanMac::MlmeSetRequest,
                                           uartNetDevice->GetMac(),
                                           MacPibAttributeIdentifier::macShortAddress,
                                           pibAttr1);

            // MLME-START.request
            MlmeStartRequestParams startParams;
            startParams.m_PanId = 0xCAFE;
            startParams.m_logCh = 11;
            startParams.m_logChPage = 0;
            startParams.m_bcnOrd = 15;
            startParams.m_sfrmOrd = 15;
            startParams.m_panCoor = true;
            startParams.m_battLifeExt = false;
            startParams.m_coorRealgn = false;

            Simulator::ScheduleWithContext(uartNetDevice->GetNode()->GetId(),
                                           Seconds(1),
                                           &UartLrWpanMac::MlmeStartRequest,
                                           uartNetDevice->GetMac(),
                                           startParams);
        }
        else
        {
            MlmeAssociateRequestParams associateParams;
            associateParams.m_chNum = 11;
            associateParams.m_chPage = 0;
            associateParams.m_coordAddrMode = AddressMode::SHORT_ADDR;
            associateParams.m_coordPanId = 0xCAFE;
            associateParams.m_capabilityInfo = 0x80; // Assign short address
            associateParams.m_coordShortAddr = Mac16Address("00:00");

            Simulator::ScheduleWithContext(uartNetDevice->GetNode()->GetId(),
                                           Seconds(1 + node->GetId()),
                                           &UartLrWpanMac::MlmeAssociateRequest,
                                           uartNetDevice->GetMac(),
                                           associateParams);
        }

        // Schedule devices data transmission after association have taken place
        if (node->GetId() != 0)
        {
            // Test MCPS-DATA.request
            McpsDataRequestParams dataParams;
            dataParams.m_dstPanId = 0xCAFE;
            dataParams.m_dstAddrMode = SHORT_ADDR;
            dataParams.m_dstAddr = Mac16Address("00:00");
            dataParams.m_msduHandle = 1;
            dataParams.m_txOptions = 0;
            dataParams.m_srcAddrMode = SHORT_ADDR;

            std::ostringstream msg;
            msg << "Hello World ns-3" << '\0';
            Ptr<Packet> packet = Create<Packet>((uint8_t*)msg.str().c_str(), msg.str().length());

            Simulator::ScheduleWithContext(uartNetDevice->GetNode()->GetId(),
                                           Seconds(nodes.GetN() + node->GetId()),
                                           &UartLrWpanMac::McpsDataRequest,
                                           uartNetDevice->GetMac(),
                                           dataParams,
                                           packet);
        }
    }

    Simulator::Stop(Seconds(20));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
