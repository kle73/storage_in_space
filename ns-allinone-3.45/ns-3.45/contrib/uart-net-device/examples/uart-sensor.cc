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

#include <iomanip>
#include <iostream>

using namespace ns3;
using namespace ns3::lrwpan;
using namespace ns3::uart;

static void
SensorIndication(Ptr<UartLrWpanNetDevice> device, SensorIndicationParams params)
{
    if (params.m_status == MacStatus::SUCCESS && params.m_type == SensorType::TEMPERATURE_HUMIDITY)
    {
        std::cout << Simulator::Now().As(Time::S) << " Temperature " << std::setprecision(5)
                  << std::fixed << params.m_temperature / 1000.0 << " C | "
                  << "Humidity: " << params.m_humidity / 1000.0 << "% RH\n";
    }
    else
    {
        std::cout << "Error while fetching sensor data from the device\n";
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
    uartNetDevice->GetMac()->SetSensorIndicationCallback(
        MakeBoundCallback(&SensorIndication, uartNetDevice));

    SensorRequestParams params;
    params.m_type = SensorType::TEMPERATURE_HUMIDITY;
    Simulator::ScheduleWithContext(uartNetDevice->GetNode()->GetId(),
                                   Seconds(2),
                                   &UartLrWpanMac::SensorRequest,
                                   uartNetDevice->GetMac(),
                                   params);

    Simulator::Stop(Seconds(5));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
