/*
 * Copyright (c) 2011 The Boeing Company
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Drishti Oza
 */

#include "ns3/abort.h"
#include "ns3/boolean.h"
#include "ns3/callback.h"
#include "ns3/command-line.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/core-module.h"
#include "ns3/double.h"
#include "ns3/gnuplot.h"
#include "ns3/log.h"
#include "ns3/mobility-model.h"
#include "ns3/mobility-module.h"
#include "ns3/multi-model-spectrum-channel.h"
#include "ns3/net-device.h"
#include "ns3/network-module.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"
#include "ns3/pointer.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-module.h"
#include "ns3/simulator.h"
#include "ns3/single-model-spectrum-channel.h"
#include "ns3/spectrum-module.h"
#include "ns3/spectrum-value.h"
#include "ns3/string.h"
#include "ns3/test.h"
#include "ns3/uinteger.h"
#include "ns3/wban-error-model.h"
#include "ns3/wban-module.h"
#include "ns3/wban-net-device.h"
#include "ns3/wban-phy.h"
#include "ns3/wban-propagation-model.h"
#include "ns3/wban-spectrum-value-helper.h"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
using namespace ns3;
using namespace std;
using namespace ns3::wban;
NS_LOG_COMPONENT_DEFINE("WbanPropagationLoss");

uint32_t g_packetsReceived = 0; //!< number of packets received

/**
 * Function called when a the PHY state change is confirmed
 * @param status PHY state
 */
void
GetSetTRXStateConfirm(WbanPhyState status)
{
    NS_LOG_UNCOND("At: " << Simulator::Now() << " Received Set TRX Confirm: " << status);
}

/**
 * Function called when a Data indication is invoked
 * @param params MCPS data indication parameters
 * @param p packet
 */
void
ReceivePhyDataIndication(uint32_t psduLength, Ptr<Packet> p, uint8_t packetSize)
{
    g_packetsReceived++;
}

int
main(int argc, char* argv[])
{
    LogComponentEnableAll(LogLevel(LOG_PREFIX_FUNC | LOG_PREFIX_NODE | LOG_PREFIX_TIME));
    LogComponentEnable("PropagationLossModel", LOG_LEVEL_DEBUG);
    LogComponentEnable("WbanPropagationLossModel", LOG_LEVEL_DEBUG);

    int packetSize = 264; // bytes (psdu payload)
    double txPower = 87;  // dBm
    uint32_t channelNumber = 1;
    double rxSensitivity = -113.97; // dBm

    CommandLine cmd(__FILE__);

    cmd.AddValue("txPower", "transmit power (dBm)", txPower);

    cmd.AddValue("channelNumber", "channel number", channelNumber);
    cmd.AddValue("rxSensitivity", "the rx sensitivity (dBm)", rxSensitivity);
    cmd.Parse(argc, argv);

    Ptr<Node> n0 = CreateObject<Node>();
    Ptr<Node> n1 = CreateObject<Node>();
    Ptr<WbanNetDevice> dev0 = CreateObject<WbanNetDevice>();
    Ptr<WbanNetDevice> dev1 = CreateObject<WbanNetDevice>();

    Ptr<SingleModelSpectrumChannel> channel = CreateObject<SingleModelSpectrumChannel>();
    Ptr<BodyPropagationLossModel> propModelBody = CreateObject<BodyPropagationLossModel>();
    Ptr<LogDistancePropagationLossModel> propModel =
        CreateObject<LogDistancePropagationLossModel>();
    Ptr<RandomPropagationLossModel> propModel2 = CreateObject<RandomPropagationLossModel>();

    channel->AddPropagationLossModel(propModelBody);
    channel->AddPropagationLossModel(propModel2);
    channel->AddPropagationLossModel(propModel);

    propModelBody->SetBodyOptions(BodyOrganOption::HEART_402_MHZ);

    dev0->SetChannel(channel);
    dev1->SetChannel(channel);
    n0->AddDevice(dev0);
    n1->AddDevice(dev1);
    Ptr<ConstantPositionMobilityModel> mob0 = CreateObject<ConstantPositionMobilityModel>();
    dev0->GetPhy()->SetMobility(mob0);
    Ptr<ConstantPositionMobilityModel> mob1 = CreateObject<ConstantPositionMobilityModel>();
    dev1->GetPhy()->SetMobility(mob1);
    mob0->SetPosition(Vector(0, 0, 0));
    mob1->SetPosition(Vector(0, 0, 0));

    WbanSpectrumValueHelper svh;
    Ptr<SpectrumValue> psd = svh.CreateTxPowerSpectralDensity(txPower, channelNumber);
    dev0->GetPhy()->SetTxPowerSpectralDensity(psd);

    // Set Rx sensitivity of the receiving device
    dev1->GetPhy()->SetRxSensitivity(rxSensitivity);

    dev0->GetPhy()->SetPhySetTRXStateConfirmCallback(MakeCallback(&GetSetTRXStateConfirm));
    dev1->GetPhy()->SetPhySetTRXStateConfirmCallback(MakeCallback(&GetSetTRXStateConfirm));

    dev0->GetPhy()->PhySetTRXStateRequest(WbanPhyState::PHY_TX_ON);
    dev1->GetPhy()->PhySetTRXStateRequest(WbanPhyState::PHY_RX_ON);

    PhyDataIndicationCallback cb0;
    cb0 = MakeCallback(&ReceivePhyDataIndication);
    dev1->GetPhy()->SetPhyDataIndicationCallback(cb0);
    PhyDataConfirmCallback params;
    Ptr<Packet> p;
    p = Create<Packet>(packetSize);

    Simulator::Schedule(Seconds(1.0), &WbanPhy::PhyDataRequest, dev0->GetPhy(), packetSize, p);

    Simulator::Stop(Seconds(10.0));

    Simulator::Run();

    Simulator::Destroy();

    return 0;
}
