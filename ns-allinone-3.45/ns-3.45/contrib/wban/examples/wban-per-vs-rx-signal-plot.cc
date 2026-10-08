/*
 * Copyright (c) 2011 The Boeing Company
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Drishti Oza
 */

/*
 * This program produces a gnuplot file that plots the rx sensitivity
 * for DBPSK error model of IEEE 802.15.6 with a frequency of 402 Mhz ,863/902/950 MHz and 2400Mhz
 * for their specific data rates using noise floor and noise figure
 */

#include "ns3/abort.h"
#include "ns3/callback.h"
#include "ns3/command-line.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/gnuplot.h"
#include "ns3/log.h"
#include "ns3/multi-model-spectrum-channel.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/simulator.h"
#include "ns3/single-model-spectrum-channel.h"
#include "ns3/test.h"
#include "ns3/uinteger.h"
#include "ns3/wban-error-model.h"
#include "ns3/wban-phy.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace ns3;
using namespace ns3::wban;

int
main(int argc, char* argv[])
{
    LogComponentEnableAll(LogLevel(LOG_PREFIX_FUNC | LOG_PREFIX_NODE | LOG_PREFIX_TIME));

    std::ostringstream os;
    std::ofstream perfile("802.15.6-per-vs-rx.plt");

    double minRxSignal = -130;
    double maxRxSignal = -89; // dBm
    double increment = 0.001;
    int packetSize = 255 * 8;
    double txPower = 0; // dBm

    double signal = 0;
    //  double nF = 19;// Noise figure in dB

    uint32_t channelNumber = 11;

    CommandLine cmd(__FILE__);

    cmd.AddValue("txPower", "transmit power (dBm)", txPower);
    cmd.AddValue("packetSize", "packet (MSDU) size (bytes)", packetSize);
    cmd.AddValue("channelNumber", "channel number", channelNumber);
    cmd.Parse(argc, argv);

    Gnuplot perplot = Gnuplot("802.15.6-per-vs-rx.eps");
    Gnuplot2dDataset perdataset402Mhz75Kbps("402 Mhz(75.9 kbps)");
    Gnuplot2dDataset perdataset402Mhz151Kbps("402 Mhz(151.8 kbps)");
    Gnuplot2dDataset perdataset863Mhz101Kbps("863/902/950 Mhz(101.2 kbps)");
    Gnuplot2dDataset perdataset863Mhz202Kbps("863/902/950 Mhz(202.4 kbps)");
    Gnuplot2dDataset perdataset2400Mhz121Kbps("2.4 Ghz(121.4 kbps)");
    Gnuplot2dDataset perdataset2400Mhz242Kbps("2.4 Ghz(242.9 kbps)");

    Ptr<WbanErrorModel> WbanError = CreateObject<WbanErrorModel>();

    // double noise2400Mhz = pow (10.0,(-114  + nF )/ 10.0) / 1000.0; //noise = noise floor for
    // Bandwidth 1 Mhz + noise figure of 19 dB double noise402Mhz = pow (10.0,(-119 +  nF )/ 10.0) /
    // 1000.0; // noise = noise floor for bandwidth 300 Khz + noise figure of 19 dB double
    // noise863Mhz = pow (10.0,(-118 +  nF )/ 10.0) / 1000.0; // noise = noise floor for bandwidth
    // 300 Khz + noise figure of 19 dB

    double r1 = 10 * log10(75900); // 402 MHZ data rate 75.9 kbps

    double noiseA =
        pow(10.0, (-173.97502677 + 19 + r1) / 10.0) / 1000.0; // -173.97502677 (1 Hz thermal noise)
    for (double j = minRxSignal; j < maxRxSignal;)
    {
        signal = pow(10.0, j / 10.0) / 1000.0; // signal in Watts
        double snr = signal / noiseA;
        // PER = 1 - (1 - BER)^nbits
        double perTheoretical =
            (1.0 - WbanError->GetChunkSuccessRatePervsRxSignalExample(snr, 255 * 8)) *
            100; // 20 bytes packets
        std::cout << "402 MHZ data rate 75.9 kbps || Theoretical Test || Signal: " << j
                  << " dBm | SNR: " << snr << "| PER " << perTheoretical << " % \n"
                  << std::setprecision(20);

        j += increment;
        perdataset402Mhz75Kbps.Add(j, perTheoretical);
    }

    double r2 = 10 * log10(151800); // 402 MHz data rate 151.8 kbps
    double noise2 =
        pow(10.0, (-173.97502677 + 19 + r2) / 10.0) / 1000.0; // -173.97502677 (1 Hz thermal noise)

    for (double j = minRxSignal; j < maxRxSignal;)
    {
        signal = pow(10.0, j / 10.0) / 1000.0; // signal in Watts
        double snr = signal / noise2;
        // PER = 1 - (1 - BER)^nbits
        double perTheoretical =
            (1.0 - WbanError->GetChunkSuccessRatePervsRxSignalExample(snr, 255 * 8)) *
            100; // 20 bytes packets
        std::cout << "402 MHz data rate 151.8 kbps || Theoretical Test || Signal: " << j
                  << " dBm | SNR: " << snr << "| PER" << perTheoretical << " % \n "
                  << std::setprecision(20);
        j += increment;
        perdataset402Mhz151Kbps.Add(j, perTheoretical);
    }

    double r3 = 10 * log10(101200); // 863 MHz data rate 101.2 kbps

    double noise3 =
        pow(10.0, (-173.97502677 + 19 + r3) / 10.0) / 1000.0; // -173.97502677 (1 Hz thermal noise)
    for (double j = minRxSignal; j < maxRxSignal;)
    {
        signal = pow(10.0, j / 10.0) / 1000.0; // signal in Watts
        double snr = signal / noise3;
        // PER = 1 - (1 - BER)^nbits
        double perTheoretical =
            (1.0 - WbanError->GetChunkSuccessRatePervsRxSignalExample(snr, 255 * 8)) *
            100; // 20 bytes packets
        std::cout << "863 MHz data rate 101.2 kbps || Theoretical Test || Signal: " << j
                  << " dBm | SNR: " << snr << "| PER " << perTheoretical << " % \n "
                  << std::setprecision(20);

        j += increment;
        perdataset863Mhz101Kbps.Add(j, perTheoretical);
    }

    double r4 = 10 * log10(202400); // 863 MHz data rate 202.4  kbps

    double noise4 =
        pow(10.0, (-173.97502677 + 19 + r4) / 10.0) / 1000.0; // -173.97502677 (1 Hz thermal noise)
    for (double j = minRxSignal; j < maxRxSignal;)
    {
        signal = pow(10.0, j / 10.0) / 1000.0; // signal in Watts
        double snr = signal / noise4;
        // PER = 1 - (1 - BER)^nbits
        double perTheoretical =
            (1.0 - WbanError->GetChunkSuccessRatePervsRxSignalExample(snr, 255 * 8)) *
            100; // 20 bytes packets
        std::cout << "863 MHz data rate 202.4  kbps || Theoretical Test || Signal: " << j
                  << " dBm | SNR: " << snr << "| PER" << perTheoretical << " % \n "
                  << std::setprecision(20);

        j += increment;
        perdataset863Mhz202Kbps.Add(j, perTheoretical);
    }

    double r5 = 10 * log10(121400); // 2400 MHz data rate 121.4 kbps

    double noise5 =
        pow(10.0, (-173.97502677 + 19 + r5) / 10.0) / 1000.0; // -173.97502677 (1 Hz thermal noise)
    for (double j = minRxSignal; j < maxRxSignal;)
    {
        signal = pow(10.0, j / 10.0) / 1000.0; // signal in Watts
        double snr = signal / noise5;
        // PER = 1 - (1 - BER)^nbits
        double perTheoretical =
            (1.0 - WbanError->GetChunkSuccessRatePervsRxSignalExample(snr, 255 * 8)) *
            100; // 20 bytes packets
        std::cout << "2400 MHz data rate 121.4 kbps || Theoretical Test || Signal: " << j
                  << " dBm | SNR: " << snr << "| PER " << perTheoretical << " % \n"
                  << std::setprecision(20);

        j += increment;
        perdataset2400Mhz121Kbps.Add(j, perTheoretical);
    }

    double r6 = 10 * log10(242900); // 2400 MHz data rate 242.9 kbps

    double noise6 =
        pow(10.0, (-173.97502677 + 19 + r6) / 10.0) / 1000.0; // -173.97502677 (1 Hz thermal noise)
    for (double j = minRxSignal; j < maxRxSignal;)
    {
        signal = pow(10.0, j / 10.0) / 1000.0; // signal in Watts
        double snr = signal / noise6;
        // PER = 1 - (1 - BER)^nbits
        double perTheoretical =
            (1.0 - WbanError->GetChunkSuccessRatePervsRxSignalExample(snr, 255 * 8)) *
            100; // 20 bytes packets
        std::cout << "2400 MHz data rate 242.9 kbps || Theoretical Test || Signal: " << j
                  << " dBm | SNR: " << snr << "| PER " << perTheoretical << " % \n "
                  << std::setprecision(20);

        j += increment;
        perdataset2400Mhz242Kbps.Add(j, perTheoretical);
    }

    perplot.AddDataset(perdataset402Mhz75Kbps);
    perplot.AddDataset(perdataset402Mhz151Kbps);
    perplot.AddDataset(perdataset863Mhz101Kbps);
    perplot.AddDataset(perdataset863Mhz202Kbps);
    perplot.AddDataset(perdataset2400Mhz121Kbps);
    perplot.AddDataset(perdataset2400Mhz242Kbps);

    perplot.SetTitle(os.str());
    perplot.SetTerminal("postscript eps color enh \"Times-BoldItalic\"");
    perplot.SetLegend("Rx signal (dBm)", "PSDU Packet Error Rate (%)");
    perplot.SetExtra("set xrange [-98:-85]\n\
                      set logscale y\n\
                      set yrange [1:100]\n\
                      set xtics 5\n\
                      set grid\n\
                      set key top right\n\
                      set style line 1 linewidth 5\n\
                      set style line 2 linewidth 5\n\
                      set style line 3 linewidth 5\n\
                      set style line 4 linewidth 5\n\
                      set style line 5 linewidth 5\n\
                      set style line 6 linewidth 5 lc 7");
    perplot.GenerateOutput(perfile);
    perfile.close();

    Simulator::Destroy();
    return 0;
}
