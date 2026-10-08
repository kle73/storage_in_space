/*
 * Copyright (c) 2011 The Boeing Company
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Drishti Oza
 */

/* This program produces a gnuplot file that plots the packet error rate vs eb/no for PLCP data
 * rates for DBPSK error model of IEEE 802.15.6 with a frequency of 402 Mhz ,863/902/950 MHz and
 * 2400Mhz and code rates BCH (31, 19) and BCH (63, 51) for their specific data rates.
 */
#include "ns3/abort.h"
#include "ns3/command-line.h"
#include "ns3/gnuplot.h"
#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"
#include "ns3/wban-error-model.h"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace ns3;
using namespace std;
using namespace ns3::wban;
NS_LOG_COMPONENT_DEFINE("WbanPerModePlcpPlot");

//
// Plot 802.15.6 BER curve
//
int
main(int argc, char* argv[])
{
    std::ofstream perfile("802.15.6-per-plcp.plt");
    Ptr<WbanErrorModel> WbanError = CreateObject<WbanErrorModel>();

    double increment = 0.01;
    double minSnr = -10; // dB
    double maxSnr = 20;
    int size = 31;
    double snr = 0;

    Gnuplot perplot = Gnuplot("802.15.6-per-plcp.eps");

    Gnuplot2dDataset perdataset402Mhz57Kbps(" 402 MHz 57.5 Kbps(PLCP), BCH 31,19,2");
    perdataset402Mhz57Kbps.SetStyle(Gnuplot2dDataset::LINES_POINTS);
    Gnuplot2dDataset perdatasetUncoded402Mhz57Kbps(" 402 MHz 57.5 Kbps(PLCP), Uncoded");
    perdatasetUncoded402Mhz57Kbps.SetStyle(Gnuplot2dDataset::LINES_POINTS);

    Gnuplot2dDataset perdataset863Mhz76Kbps("863/902/950 MHz 76.6 Kbps(PLCP), BCH 31,19,2");
    perdataset863Mhz76Kbps.SetStyle(Gnuplot2dDataset::LINES_POINTS);
    Gnuplot2dDataset perdatasetUncoded863Mhz76Kbps("863/902/950 MHZ 76.6 Kbps(PLCP), Uncoded");
    perdatasetUncoded863Mhz76Kbps.SetStyle(Gnuplot2dDataset::LINES_POINTS);

    Gnuplot2dDataset perdataset2400Mhz91Kbps("2400 Mhz 91Kbps(PLCP), BCH 31,19,2");
    perdataset2400Mhz91Kbps.SetStyle(Gnuplot2dDataset::LINES_POINTS);
    Gnuplot2dDataset perdatasetUncoded2400Mhz91Kbps("2400 MHz 91Kbps(PLCP), Uncoded");
    perdatasetUncoded2400Mhz91Kbps.SetStyle(Gnuplot2dDataset::LINES_POINTS);

    for (snr = minSnr; snr <= maxSnr; snr += increment)
    {
        // 402 MHZ with data rate of 57.5 Kbps bch coded
        double per402Mhz57Kbps =
            (1.0 - WbanError->GetChunkSuccessRate402Mhz57KbpsCoded(pow(10.0, snr / 10.0), size)) *
            100;

        perdataset402Mhz57Kbps.Add(snr, per402Mhz57Kbps);

        // 402 MHZ with data rate of 57.5 Kbps uncoded
        double perUncoded402Mhz57Kbps =
            (1.0 - WbanError->GetChunkSuccessRate402Mhz57KbpsUncoded(pow(10.0, snr / 10.0), size)) *
            100;

        perdatasetUncoded402Mhz57Kbps.Add(snr, perUncoded402Mhz57Kbps);

        // 863 MHz to 870 MHz or 902 MHz to 928 MHz or 950 MHz to 958 MHz with data rate of 76.6
        // Kbps bch coded
        double per863Mhz76Kbps =
            (1.0 - WbanError->GetChunkSuccessRate863Mhz76KbpsCoded(pow(10.0, snr / 10.0), size)) *
            100;

        perdataset863Mhz76Kbps.Add(snr, per863Mhz76Kbps);

        // 863 MHz to 870 MHz or 902 MHz to 928 MHz or 950 MHz to 958 MHz with data rate of 76.6
        // Kbps uncoded
        double perUncoded863Mhz76Kbps =
            (1.0 - WbanError->GetChunkSuccessRate863Mhz76KbpsUncoded(pow(10.0, snr / 10.0), size)) *
            100;

        perdatasetUncoded863Mhz76Kbps.Add(snr, perUncoded863Mhz76Kbps);

        // 2400 MHz to 2483.5 MHz or 2360 MHz to 2400 MHz with data rate of 91.9 Kbps bch coded
        double per2400Mhz91Kbps =
            (1.0 - WbanError->GetChunkSuccessRate2400Mhz91KbpsCoded(pow(10.0, snr / 10.0), size)) *
            100;

        perdataset2400Mhz91Kbps.Add(snr, per2400Mhz91Kbps);

        // 2400 MHz to 2483.5 MHz or 2360 MHz to 2400 MHz with data rate of 91.9 Kbps uncoded
        double perUncoded2400Mhz91Kbps =
            (1.0 -
             WbanError->GetChunkSuccessRate2400Mhz91KbpsUncoded(pow(10.0, snr / 10.0), size)) *
            100;

        perdatasetUncoded2400Mhz91Kbps.Add(snr, perUncoded2400Mhz91Kbps);
    }

    perplot.AddDataset(perdataset402Mhz57Kbps);
    perplot.AddDataset(perdatasetUncoded402Mhz57Kbps);
    perplot.AddDataset(perdataset863Mhz76Kbps);
    perplot.AddDataset(perdatasetUncoded863Mhz76Kbps);
    perplot.AddDataset(perdataset2400Mhz91Kbps);
    perplot.AddDataset(perdatasetUncoded2400Mhz91Kbps);

    perplot.SetTerminal("postscript eps color enh \"Times-BoldItalic\"");
    perplot.SetLegend("(dB)", "PLCP Packet Error Rate (%)");
    perplot.SetExtra("set xrange [-12:6]\n\
set yrange [1:100]\n\
set log y\n\
set grid\n\
set key reverse bottom left\n\
set linetype  1 dt 3 lw 2 lc 1 pt 1 pi -25 ps 1\n\
set linetype  2 dt 3 lw 2 lc 2 pt 2 pi -26 ps 1\n\
set linetype  3 dt 3 lw 2 lc 3 pt 3 pi -27 ps 1\n\
set linetype  4 dt 3 lw 2 lc 4 pt 7 pi -26 ps 1\n\
set linetype  5 dt 3 lw 2 lc 5 pt 5 pi -26 ps 1\n\
set linetype  6 dt 3 lw 2 lc 6 pt 6 pi -27 ps 1");

    perplot.GenerateOutput(perfile);
    perfile.close();

    return 0;
}
