/*
 * Copyright (c) 2011 The Boeing Company
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Drishti Oza
 */

#include "ns3/command-line.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/error-rate-model.h"
#include "ns3/log.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/single-model-spectrum-channel.h"
#include "ns3/test.h"
#include "ns3/wban-error-model.h"
#include "ns3/wban-phy.h"

using namespace ns3;
using namespace ns3::wban;

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
 * Function called when a the PHY receives a packet
 * @param psduLength PSDU length
 * @param p packet
 * @param lqi link quality indication
 */
void
ReceivePhyDataIndication(uint32_t psduLength, Ptr<Packet> p, uint8_t lqi)
{
    NS_LOG_UNCOND("At: " << Simulator::Now() << " Received frame size: " << psduLength
                         << " LQI: " << (uint16_t)lqi);
}

/**
 * Send one packet
 * @param sender sender PHY
 * @param receiver receiver PHY
 */
void
SendOnePacket(Ptr<WbanPhy> sender, Ptr<WbanPhy> receiver)
{
    uint32_t n = 10;
    Ptr<Packet> p = Create<Packet>(n);
    sender->PhyDataRequest(p->GetSize(), p);
}

int
main(int argc, char* argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    LogComponentEnableAll(LogLevel(LOG_PREFIX_TIME | LOG_PREFIX_FUNC | LOG_PREFIX_NODE));
    LogComponentEnable("WbanPhy", LOG_LEVEL_DEBUG);

    Ptr<WbanPhy> sender = CreateObject<WbanPhy>();
    Ptr<WbanPhy> receiver = CreateObject<WbanPhy>();

    Ptr<SingleModelSpectrumChannel> channel = CreateObject<SingleModelSpectrumChannel>();
    sender->SetChannel(channel);
    receiver->SetChannel(channel);
    channel->AddRx(sender);
    channel->AddRx(receiver);

    // CONFIGURE MOBILITY
    Ptr<ConstantPositionMobilityModel> senderMobility =
        CreateObject<ConstantPositionMobilityModel>();
    sender->SetMobility(senderMobility);
    Ptr<ConstantPositionMobilityModel> receiverMobility =
        CreateObject<ConstantPositionMobilityModel>();
    receiver->SetMobility(receiverMobility);

    Ptr<WbanErrorModel> e = CreateObject<WbanErrorModel>();
    sender->SetErrorModel(e);
    receiver->SetErrorModel(e);

    sender->SetPhySetTRXStateConfirmCallback(MakeCallback(&GetSetTRXStateConfirm));
    receiver->SetPhySetTRXStateConfirmCallback(MakeCallback(&GetSetTRXStateConfirm));

    sender->PhySetTRXStateRequest(WbanPhyState::PHY_TX_ON);
    receiver->PhySetTRXStateRequest(WbanPhyState::PHY_RX_ON);

    receiver->SetPhyDataIndicationCallback(MakeCallback(&ReceivePhyDataIndication));

    Simulator::Schedule(Seconds(1.0), &SendOnePacket, sender, receiver);

    Simulator::Stop(Seconds(10.0));

    Simulator::Run();

    Simulator::Destroy();

    return 0;
}
