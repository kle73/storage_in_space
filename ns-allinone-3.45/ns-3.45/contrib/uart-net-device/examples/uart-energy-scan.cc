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
#include "ns3/lr-wpan-module.h"
#include "ns3/network-module.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/uart-lr-wpan-net-device.h"

#include <iostream>

using namespace ns3;
using namespace ns3::lrwpan;
using namespace ns3::uart;

/*static void
ScanConfirm(Ptr<LrWpanNetDevice> device, MlmeScanConfirmParams params)
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
}
*/
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
}

int
main(int argc, char* argv[])
{
    // We are using a real piece of hardware, therefore we need to use realtime
    GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::RealtimeSimulatorImpl"));
    // Enable calculation of FCS in the trailers. Only necessary when interacting with real devices
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));

    Ptr<Node> node = CreateObject<Node>();
    Ptr<UartLrWpanNetDevice> uartNetDevice = CreateObject<UartLrWpanNetDevice>("/dev/ttyUSB0");
    node->AddDevice(uartNetDevice);

    // Device Confirm primitives callback hooks
    uartNetDevice->GetMac()->SetMlmeScanConfirmCallback(
        MakeBoundCallback(&ScanConfirm, uartNetDevice));

    // Test MLME-SCAN.request (ENERGY DETECTION)
    MlmeScanRequestParams scanParams;
    scanParams.m_scanChannels = 0x00007800; // Channels 11-14
    scanParams.m_scanDuration = 3;
    scanParams.m_scanType = MLMESCAN_ED;
    Simulator::ScheduleWithContext(uartNetDevice->GetNode()->GetId(),
                                   Seconds(2),
                                   &UartLrWpanMac::MlmeScanRequest,
                                   uartNetDevice->GetMac(),
                                   scanParams);

    /*Ptr<Node> node = CreateObject<Node>();
    Ptr<LrWpanNetDevice> lrWpanNetdevice = Create<LrWpanNetDevice> ();
    node->AddDevice(lrWpanNetdevice);

     lrWpanNetdevice ->GetMac()->SetMlmeScanConfirmCallback(
        MakeBoundCallback(&ScanConfirm, lrWpanNetdevice));

    MlmeScanRequestParams scanParams;
    scanParams.m_scanChannels = 0x00007800; // Channels 11-14
    scanParams.m_scanDuration = 3;
    scanParams.m_scanType = MLMESCAN_ED;
    Simulator::ScheduleWithContext(lrWpanNetdevice->GetNode()->GetId(),
                                   Seconds(2),
                                   &LrWpanMac::MlmeScanRequest,
                                   lrWpanNetdevice->GetMac(),
                                   scanParams);*/

    Simulator::Stop(Seconds(5));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
