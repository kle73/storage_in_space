/*
 * Copyright (c) 2011 The Boeing Company
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author:
 */

/*
 * This program produces a gnuplot file that plots the theoretical and experimental packet error
 * rate (PER) as a function of receive signal for the 802.15.6 model. As described by the standard,
 * the PER is calculated with the transmission of frames with a PSDU of 24 bytes. In the
 * experimental test, 1000 frames are transmitted for each Rx signal ranging from -130 dBm to -100
 * dBm with increments of 0.01 dBm. The point before PER is < 10 % is the device receive
 * sensitivity. Theoretical and experimental Rx sensitivity is printed at the end of the end and a
 * plot is generated.
 */

#include "ns3/core-module.h"
#include "ns3/gnuplot.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/propagation-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/wban-error-model.h"
#include "ns3/wban-module.h"
#include "ns3/wban-phy.h"
#include "ns3/wban-propagation-model.h"

#include <iomanip>

using namespace ns3;
using namespace ns3::wban;
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
    // LogComponentEnable("WbanPhy", LOG_LEVEL_DEBUG);

    std::ostringstream os;
    std::ofstream perfile("802.15.6-per-vs-rx.plt");

    double minRxSignal = -120; // 150 dBm
    double maxRxSignal = -100; // dBm
    double increment = 0.01;
    int maxPackets = 1000;
    int packetSize = 264; // bytes (psdu payload)
    double txPower = 0;   // dBm
    uint32_t channelNumber = 1;
    double rxSensitivity = -113.97; // dBm

    CommandLine cmd(__FILE__);

    cmd.AddValue("txPower", "transmit power (dBm)", txPower);
    cmd.AddValue("packetSize", "packet (PSDU) size (bytes)", packetSize);
    cmd.AddValue("channelNumber", "channel number", channelNumber);
    cmd.AddValue("rxSensitivity", "the rx sensitivity (dBm)", rxSensitivity);
    cmd.Parse(argc, argv);

    Gnuplot perplot = Gnuplot("802.15.6-per-vs-rxSignal.eps");
    Gnuplot2dDataset perdatasetExperimental("Experimental");
    Gnuplot2dDataset perdatasetTheoretical("Theoretical");

    Ptr<Node> n0 = CreateObject<Node>();
    Ptr<Node> n1 = CreateObject<Node>();
    Ptr<WbanNetDevice> dev0 = CreateObject<WbanNetDevice>();
    Ptr<WbanNetDevice> dev1 = CreateObject<WbanNetDevice>();

    dev0->SetAddress(Mac16Address("00:01"));
    dev1->SetAddress(Mac16Address("00:02"));
    Ptr<SingleModelSpectrumChannel> channel = CreateObject<SingleModelSpectrumChannel>();
    Ptr<FixedRssLossModel> propModel = CreateObject<FixedRssLossModel>();
    channel->AddPropagationLossModel(propModel);

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

    //////////////////////////////////
    // Experimental  PER v.s Signal //
    //////////////////////////////////

    double per = 1;
    double sensitivityExp = 0;
    bool sensThreshold = true;

    for (double j = minRxSignal; j < maxRxSignal; j += increment)
    {
        propModel->SetRss(j);
        if (sensThreshold)
        {
            sensitivityExp = j;
        }

        for (int i = 0; i < maxPackets; i++)
        {
            PhyDataConfirmCallback params;
            Ptr<Packet> p;
            p = Create<Packet>(packetSize);
            Simulator::Schedule(Seconds(i),
                                &WbanPhy::PhyDataRequest,
                                dev0->GetPhy(),
                                packetSize,
                                p);
        }

        Simulator::Run();

        per = (static_cast<double>(maxPackets - g_packetsReceived) / maxPackets) * 100;

        std::cout << "Experimental Test || Signal: " << j << " dBm | Received " << g_packetsReceived
                  << " pkts"
                  << "/" << maxPackets << " | PER " << per << " %\n";

        if (per <= 10 && sensThreshold)
        {
            sensThreshold = false;
        }

        perdatasetExperimental.Add(j, per);
        g_packetsReceived = 0;
    }

    /////////////////////////////////
    // Theoretical PER v.s. Signal //
    /////////////////////////////////
    double sensitivityTheo = 0;
    Ptr<WbanErrorModel> WbanError = CreateObject<WbanErrorModel>();
    WbanSpectrumValueHelper psdHelper;
    double signal = 0;

    double maxRxSensitivityW = (pow(10.0, -113.97 / 10.0) / 1000.0);
    long double noiseFactor = (pow(10.0, (rxSensitivity) / 10.0) / 1000.0) / maxRxSensitivityW;
    psdHelper.SetNoiseFactor(noiseFactor);
    Ptr<const SpectrumValue> noisePsd = psdHelper.CreateNoisePowerSpectralDensity(11);
    double noise = WbanSpectrumValueHelper::TotalAvgPower(noisePsd, 11);
    std::cout << "noise = " << noise;
    for (double j = minRxSignal; j < maxRxSignal;)
    {
        if (sensThreshold)
        {
            sensitivityTheo = j;
        }
        signal = pow(10.0, j / 10.0) / 1000.0; // signal in Watts
        double snr = signal / noise;
        // PER = 1 - (1 - BER)^nbits
        double perTheoretical =
            (1.0 - WbanError->GetChunkSuccessRate2400Mhz242KbpsUncoded(snr, (packetSize * 8))) *
            100; // 255 bytes packets
        std::cout << "Theoretical Test || Signal: " << j << " dBm | SNR: " << snr << "| PER "
                  << perTheoretical << " % \n"
                  << std::setprecision(20);
        j += increment;
        if (perTheoretical <= 10 && sensThreshold)
        {
            sensThreshold = false;
        }
        perdatasetTheoretical.Add(j, perTheoretical);
    }
    std::cout << "_____________________________________________________________________________\n";
    std::cout << "Experimental Test || Receiving with a current sensitivity of " << sensitivityExp
              << " dBm\n";
    std::cout << "Theoretical Test  || Receiving with a current sensitivity of " << sensitivityTheo
              << " dBm\n";
    std::cout << "Gnu plot generated.";

    os << "Pkt Payload (PSDU) size = " << packetSize << " bytes | "
       << "Tx power = " << txPower << " dBm | "
       << "Frequency = 2.4 Ghz ";

    perplot.AddDataset(perdatasetExperimental);
    perplot.AddDataset(perdatasetTheoretical);

    perplot.SetTitle(os.str());
    perplot.SetTerminal("postscript eps color enh \"Times-BoldItalic\"");
    perplot.SetLegend("Rx signal (dBm)", "Packet Error Rate (%)");
    perplot.SetExtra("set xrange [-120:-100]\n\
                      set logscale y\n\
                      set yrange [1:100]\n\
                      set xtics 2\n\
                      set grid\n\
                      set style line 1 linewidth 25\n\
                      set style line 2 linewidth 3\n\
                      set arrow from -120,10 to -100,10 nohead lc 'web-blue' front");
    perplot.GenerateOutput(perfile);
    perfile.close();

    Simulator::Destroy();
    return 0;
}
