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
#include "ns3/simulator.h"
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
    std::cout
        << Simulator::Now().As(Time::S) << " Node " << device->GetNode()->GetId()
        << ", AssociateIndication: Coordinator Received Association request from device with\n";
    std::cout << "capability " << std::hex << static_cast<uint32_t>(params.capabilityInfo)
              << std::dec << " | Dev Addr: " << params.m_extDevAddr << "\n";
    std::cout << "Sending Association Response...\n";

    MlmeAssociateResponseParams respParams;

    CapabilityField capability;
    capability.SetCapability(params.capabilityInfo);
    // if (capability.IsShortAddrAllocOn())
    //{
    // Associated with address assignation (static address AB:CD)
    respParams.m_assocShortAddr = Mac16Address("AB:CD");
    /* }
     else
     {
        // Associated without address assignation
        respParams.m_assocShortAddr = Mac16Address("FF:FE");
     }*/

    respParams.m_extDevAddr = params.m_extDevAddr;
    respParams.m_status = MacStatus::SUCCESS;

    device->GetMac()->MlmeAssociateResponse(respParams);
}

static void
CommStatusIndication(Ptr<UartLrWpanNetDevice> device, MlmeCommStatusIndicationParams params)
{
    std::cout << Simulator::Now().As(Time::S) << " Node " << device->GetNode()->GetId()
              << ", Coordinator received COMM-STATUS.indication\n";
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
    std::cout << Simulator::Now().As(Time::S) << " Node " << device->GetNode()->GetId()
              << ", Data confirm | Status :" << static_cast<uint32_t>(params.m_status)
              << " | Msdu handle " << static_cast<uint32_t>(params.m_msduHandle) << "\n";
}

static void
DataIndication(Ptr<UartLrWpanNetDevice> device, McpsDataIndicationParams params, Ptr<Packet> p)
{
    std::vector<uint8_t> buffer;
    buffer.resize(p->GetSize());
    p->CopyData(buffer.data(), p->GetSize());
    std::string data(buffer.begin(), buffer.end());

    std::cout << Simulator::Now().As(Time::S) << " Node " << device->GetNode()->GetId()
              << ", Data Indication | Packet received with :\n"
              << "    SrcMode: " << static_cast<uint32_t>(params.m_srcAddrMode) << "\n"
              << "    SrcAddr: " << params.m_srcAddr << "\n"
              << "    SrcPanId: 0x" << std::hex << params.m_srcPanId << std::dec << "\n"
              << "    DstMode: " << static_cast<uint32_t>(params.m_dstAddrMode) << "\n"
              << "    DstAddr: " << params.m_dstAddr << "\n"
              << "    DstPanId: 0x" << std::hex << params.m_dstPanId << std::dec << "\n"
              << "    LQI: " << static_cast<uint32_t>(params.m_mpduLinkQuality) << "\n"
              << "    DSN: " << static_cast<uint32_t>(params.m_dsn) << "\n"
              << "    Msdu size: " << p->GetSize() << "\n"
              << "    Data: " << data << "\n";
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
    // We are using a real piece of hardware, therefore we need to use realtime
    GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::RealtimeSimulatorImpl"));
    // Enable calculation of FCS in the trailers. Only necessary when interacting with real devices
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));

    // Coordinator
    Ptr<Node> node = CreateObject<Node>();
    Ptr<UartLrWpanNetDevice> uartNetDevice = CreateObject<UartLrWpanNetDevice>("/dev/ttyUSB0");
    node->AddDevice(uartNetDevice);

    // End Device
    Ptr<Node> node2 = CreateObject<Node>();
    Ptr<UartLrWpanNetDevice> uartNetDevice2 = CreateObject<UartLrWpanNetDevice>("/dev/ttyUSB1");
    node2->AddDevice(uartNetDevice2);

    // Coordinator Confirm and Indication primitives callback hooks
    uartNetDevice->GetMac()->SetMlmeStartConfirmCallback(
        MakeBoundCallback(&StartConfirm, uartNetDevice));
    uartNetDevice->GetMac()->SetMlmeAssociateIndicationCallback(
        MakeBoundCallback(&AssociateIndication, uartNetDevice));
    uartNetDevice->GetMac()->SetMlmeCommStatusIndicationCallback(
        MakeBoundCallback(&CommStatusIndication, uartNetDevice));
    uartNetDevice->GetMac()->SetMcpsDataIndicationCallback(
        MakeBoundCallback(&DataIndication, uartNetDevice));
    uartNetDevice->GetMac()->SetMlmeSetConfirmCallback(
        MakeBoundCallback(&SetConfirm, uartNetDevice));
    uartNetDevice->GetMac()->SetMlmeGetConfirmCallback(
        MakeBoundCallback(&GetConfirm, uartNetDevice));

    // Device Confirm primitives callback hooks
    uartNetDevice2->GetMac()->SetMlmeScanConfirmCallback(
        MakeBoundCallback(&ScanConfirm, uartNetDevice2));
    uartNetDevice2->GetMac()->SetMlmeAssociateConfirmCallback(
        MakeBoundCallback(&AssociateConfirm, uartNetDevice2));
    uartNetDevice2->GetMac()->SetMcpsDataConfirmCallback(
        MakeBoundCallback(&DataConfirm, uartNetDevice2));
    uartNetDevice2->GetMac()->SetMlmeSetConfirmCallback(
        MakeBoundCallback(&SetConfirm, uartNetDevice2));
    uartNetDevice2->GetMac()->SetMlmeGetConfirmCallback(
        MakeBoundCallback(&GetConfirm, uartNetDevice2));

    // Test MLME-SET.request (page), this is basically a place holder
    // JN5169 , do not have other page other than 0
    /*Ptr<MacPibAttributes> pibAttr0 = Create<MacPibAttributes>();
    pibAttr0->pCurrentPage = 2;
    Simulator::ScheduleWithContext(uartNetDevice->GetNode()->GetId(),
                                   Seconds(1.0),
                                   &UartLrWpanMac::MlmeSetRequest,
                                   uartNetDevice->GetMac(),
                                   MacPibAttributeIdentifier::pCurrentPage,
                                   pibAttr0);

    Simulator::ScheduleWithContext(uartNetDevice->GetNode()->GetId(),
                                   Seconds(1.5),
                                   &UartLrWpanMac::MlmeGetRequest,
                                   uartNetDevice->GetMac(),
                                   MacPibAttributeIdentifier::pCurrentPage);*/

    /*
    // Test MLME-SCAN.request (ENERGY DETECTION)
    MlmeScanRequestParams scanParams;
    scanParams.m_scanChannels = 0x00a2c800; // 0x00007800; // Channels 11-14
    scanParams.m_scanDuration = 1;
    scanParams.m_scanType = MLMESCAN_ED;
    Simulator::ScheduleWithContext(uartNetDevice2->GetNode()->GetId(),
                                   Seconds(1.0),
                                   &UartLrWpanMac::MlmeScanRequest,
                                   uartNetDevice2->GetMac(),
                                   scanParams);
    */

    // Test MLME-SET.request (channel)
    Ptr<MacPibAttributes> pibAttr0 = Create<MacPibAttributes>();
    pibAttr0->pCurrentChannel = 0xD;
    Simulator::ScheduleWithContext(uartNetDevice->GetNode()->GetId(),
                                   Seconds(1.0),
                                   &UartLrWpanMac::MlmeSetRequest,
                                   uartNetDevice->GetMac(),
                                   MacPibAttributeIdentifier::pCurrentChannel,
                                   pibAttr0);

    Simulator::ScheduleWithContext(uartNetDevice->GetNode()->GetId(),
                                   Seconds(1.5),
                                   &UartLrWpanMac::MlmeGetRequest,
                                   uartNetDevice->GetMac(),
                                   MacPibAttributeIdentifier::pCurrentChannel);

    // Test MLME-SET.request (short address)
    Ptr<MacPibAttributes> pibAttr1 = Create<MacPibAttributes>();
    pibAttr1->macShortAddress = Mac16Address("00:00");
    Simulator::ScheduleWithContext(uartNetDevice->GetNode()->GetId(),
                                   Seconds(2.0),
                                   &UartLrWpanMac::MlmeSetRequest,
                                   uartNetDevice->GetMac(),
                                   MacPibAttributeIdentifier::macShortAddress,
                                   pibAttr1);

    // Test MLME-START.request
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
                                   Seconds(3.0),
                                   &UartLrWpanMac::MlmeStartRequest,
                                   uartNetDevice->GetMac(),
                                   startParams);

    // Test MLME-SET.request (beacon Payload Length and beacon Payload)
    //      MLME-GET.request
    std::string stringMsg = "My beacon Payload in ns-3\0";
    std::vector<uint8_t> beaconMsg(stringMsg.begin(), stringMsg.end());

    Ptr<MacPibAttributes> pibAttr2 = Create<MacPibAttributes>();
    pibAttr2->macBeaconPayloadLength = beaconMsg.size();
    Simulator::ScheduleWithContext(uartNetDevice->GetNode()->GetId(),
                                   Seconds(3.4),
                                   &UartLrWpanMac::MlmeSetRequest,
                                   uartNetDevice->GetMac(),
                                   MacPibAttributeIdentifier::macBeaconPayloadLength,
                                   pibAttr2);

    Simulator::ScheduleWithContext(uartNetDevice->GetNode()->GetId(),
                                   Seconds(3.6),
                                   &UartLrWpanMac::MlmeGetRequest,
                                   uartNetDevice->GetMac(),
                                   MacPibAttributeIdentifier::macBeaconPayloadLength);

    Ptr<MacPibAttributes> pibAttr3 = Create<MacPibAttributes>();
    pibAttr3->macBeaconPayload = beaconMsg; // beaconPayload;
    Simulator::ScheduleWithContext(uartNetDevice->GetNode()->GetId(),
                                   Seconds(3.8),
                                   &UartLrWpanMac::MlmeSetRequest,
                                   uartNetDevice->GetMac(),
                                   MacPibAttributeIdentifier::macBeaconPayload,
                                   pibAttr3);

    Simulator::ScheduleWithContext(uartNetDevice->GetNode()->GetId(),
                                   Seconds(3.9),
                                   &UartLrWpanMac::MlmeGetRequest,
                                   uartNetDevice->GetMac(),
                                   MacPibAttributeIdentifier::macBeaconPayload);

    // Test MLME-GET.request (macExtendedAddress)
    Simulator::ScheduleWithContext(uartNetDevice2->GetNode()->GetId(),
                                   Seconds(5.5),
                                   &UartLrWpanMac::MlmeGetRequest,
                                   uartNetDevice2->GetMac(),
                                   MacPibAttributeIdentifier::macExtendedAddress);

    Simulator::ScheduleWithContext(uartNetDevice2->GetNode()->GetId(),
                                   Seconds(5.7),
                                   &UartLrWpanMac::MlmeGetRequest,
                                   uartNetDevice2->GetMac(),
                                   MacPibAttributeIdentifier::macPanId);

    // Test MLME-SCAN.request (ACTIVE SCAN)
    MlmeScanRequestParams scanParams;
    scanParams.m_scanChannels = 0x00a2c800; // 0x00007800; // Channels 11-14
    scanParams.m_scanDuration = 3;
    scanParams.m_scanType = MLMESCAN_ACTIVE;
    Simulator::ScheduleWithContext(uartNetDevice2->GetNode()->GetId(),
                                   Seconds(4.0),
                                   &UartLrWpanMac::MlmeScanRequest,
                                   uartNetDevice2->GetMac(),
                                   scanParams);

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

    Simulator::ScheduleWithContext(uartNetDevice2->GetNode()->GetId(),
                                   Seconds(6.0),
                                   &UartLrWpanMac::McpsDataRequest,
                                   uartNetDevice2->GetMac(),
                                   dataParams,
                                   packet);

    Simulator::Stop(Seconds(12));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
