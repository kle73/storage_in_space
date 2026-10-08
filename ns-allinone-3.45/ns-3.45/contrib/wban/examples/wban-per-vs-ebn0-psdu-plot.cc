/*
 * Copyright (c) 2011 The Boeing Company
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Drishti Oza
 */

/* This program produces a gnuplot file that plots the packet error rate  vs eb/no for PSDU data
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
NS_LOG_COMPONENT_DEFINE("WbanPerModelPsduPlot");

//
// Plot 802.15.6 BER curve
//
int
main(int argc, char* argv[])
{
    std::ofstream berfile("802.15.6-psdu-per.plt");
    Ptr<WbanErrorModel> WbanError = CreateObject<WbanErrorModel>();

    double increment = 0.01;
    double minSnr = -10; // dB
    double maxSnr = 20;
    int packetSize = 255 * 8;
    double snr = 0;

    Gnuplot berplot = Gnuplot("802.15.6-psdu-per.eps");

    Gnuplot2dDataset berdataset402Mhz75Kbps("402 MHz 75.9 Kbps(PSDU), BCH 63,51,2");
    berdataset402Mhz75Kbps.SetStyle(Gnuplot2dDataset::LINES_POINTS);
    Gnuplot2dDataset berdataset402Mhz75KbpsUncoded(" 402 MHz 75.9 Kbps(PSDU), Uncoded");
    berdataset402Mhz75KbpsUncoded.SetStyle(Gnuplot2dDataset::LINES_POINTS);

    Gnuplot2dDataset berdataset863Mhz101Kbps("863/902/950 MHz 101.2 Kbps(PSDU), BCH 63,51,2");
    berdataset863Mhz101Kbps.SetStyle(Gnuplot2dDataset::LINES_POINTS);
    Gnuplot2dDataset berdataset863Mhz101KbpsUncoded("863/902/950 MHz 101.2 Kbps(PSDU), Uncoded");
    berdataset863Mhz101KbpsUncoded.SetStyle(Gnuplot2dDataset::LINES_POINTS);

    Gnuplot2dDataset berdataset2400Mhz121Kbps("204 Mhz 75.9Kbps(PSDU), BCH 63,51,2");
    berdataset2400Mhz121Kbps.SetStyle(Gnuplot2dDataset::LINES_POINTS);
    Gnuplot2dDataset berdataset2400Mhz121KbpsUncoded("2400 MHZ 121Kbps(PSDU), Uncoded");
    berdataset2400Mhz121KbpsUncoded.SetStyle(Gnuplot2dDataset::LINES_POINTS);

    for (snr = minSnr; snr <= maxSnr; snr += increment)
    {
        // 402 MHZ to 405 MHz coded with data rate of 75.9 Kbps
        double per402Mhz75Kbps =
            (1.0 -
             WbanError->GetChunkSuccessRate402Mhz75KbpsCoded(pow(10.0, snr / 10.0), packetSize)) *
            100;

        berdataset402Mhz75Kbps.Add(snr, per402Mhz75Kbps);

        // 863 MHz to 870 MHz or 902 MHz to 928 MHz or 950 MHz to 958 MHz coded with data rate of
        // 101.2 Kbps
        double per863Mhz101Kbps =
            (1.0 -
             WbanError->GetChunkSuccessRate863Mhz101KbpsCoded(pow(10.0, snr / 10.0), packetSize)) *
            100;

        berdataset863Mhz101Kbps.Add(snr, per863Mhz101Kbps);
        // 2400 Mhz to 2483.5 MHz or 2360 MHz to 2400 MHz coded with data rate 121.4 Kbps
        double per2400Mhz121Kbps =
            (1.0 - WbanError->GetChunkSuccessRate2400Mhz(pow(10.0, snr / 10.0), packetSize)) * 100;
        berdataset2400Mhz121Kbps.Add(snr, per2400Mhz121Kbps);

        // 402 MHZ to 405 MHz uncoded with data rate of 75.9 Kbps

        double perun2 =
            (1.0 -
             WbanError->GetChunkSuccessRate402Mhz75KbpsUncoded(pow(10.0, snr / 10.0), packetSize)) *
            100;

        berdataset402Mhz75KbpsUncoded.Add(snr, perun2);

        // 863 MHz to 870 MHz or 902 MHz to 928 MHz or 950 MHz to 958 MHz uncoded with data rate of
        // 101.2 Kbps

        double perun4 =
            (1.0 - WbanError->GetChunkSuccessRate863Mhz101KbpsUncoded(pow(10.0, snr / 10.0),
                                                                      packetSize)) *
            100;

        berdataset863Mhz101KbpsUncoded.Add(snr, perun4);

        // 2400 Mhz to 2483.5 MHz or 2360 MHz to 2400 MHz uncoded with data rate 121.4 Kbps
        double perun6 =
            (1.0 - WbanError->GetChunkSuccessRate2400Mhz121KbpsUncoded(pow(10.0, snr / 10.0),
                                                                       packetSize)) *
            100;

        berdataset2400Mhz121KbpsUncoded.Add(snr, perun6);
    }

    berplot.AddDataset(berdataset402Mhz75Kbps);
    berplot.AddDataset(berdataset402Mhz75KbpsUncoded);

    berplot.AddDataset(berdataset863Mhz101Kbps);
    berplot.AddDataset(berdataset863Mhz101KbpsUncoded);

    berplot.AddDataset(berdataset2400Mhz121Kbps);
    berplot.AddDataset(berdataset2400Mhz121KbpsUncoded);

    berplot.SetTerminal("postscript eps color enh \"Times-BoldItalic\"");
    berplot.SetLegend("(dB)", "PSDU Packet Error Rate (%)");
    berplot.SetExtra("set xrange [-12:6]\n\
set yrange [1:100]\n\
set log y\n\
set grid\n\
set key reverse bottom left\n\
set style line 1 lt 1 dt 3 lw 5 lc rgb 'red' pt 2 pi -25 ps 1\n\
set style line 2 linewidth 5  lc rgb 'red' pt 1 pi -26\n\
set style line 11 lt 1 dt 3 lw 5 lc  rgb 'royalblue' pt 1 pi -25 ps 1\n\
set style line 21 lt 2 dt 3 lw 5 lc 2 pt 2 pi -26 ps 1 \n\
set style line 11 lt 3 dt 7 lw 3 lc 3 pt 3 pi -27 ps 1 \n\
set style line 222 lt 4 dt 3 lw 5 lc 4 pt 4 pi -25 ps 1\n\
set style line 31 lt 5 dt 3 lw 5 lc  rgb 'dark-green' pt 5 pi -27 ps 1\n\
set style line 41 lt 1 dt 7 lw 5 lc  rgb 'orange' pt 1 pi -27 ps 1\n\
set style line 3 lt 2 dt 3 lw 5 lc 7 pt 2 pi -25 ps 1\n\
set style line 4 lt 3 dt 3 lw 5 lc 7 pt 1 pi -26 ps 1\n\
set style line 51 lt 4 dt 3 lw 5 lc 9 pt 4 pi -27 ps 1\n\
set style line 61 lt 5 dt 3 lw 5 lc 10 pt 5 pi -25 ps 1\n\
set style line 5 lt 1 dt 3 lw 5 lc  rgb 'brown' pt 2 pi -26 ps 1\n\
set style line 6 lt 2 dt 3 lw 4 lc  rgb 'brown' pt 1 pi -27 ps 1\n\
set style increment user");

    berplot.GenerateOutput(berfile);
    berfile.close();

    return 0;
}
