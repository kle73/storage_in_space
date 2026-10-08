/*
 * Copyright (c) 2011 The Boeing Company
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Drishti Oza
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
#include "ns3/wban-module.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#define PI (3.141592653589793)

using namespace ns3;
using namespace std;
using namespace ns3::wban;
NS_LOG_COMPONENT_DEFINE("WbanErrorModelPlot");

//
// Plot 802.15.6 BER curve
//
int
main(int argc, char* argv[])
{
    std::ofstream berfile("ber.plt");
    Ptr<WbanErrorModel> WbanError = CreateObject<WbanErrorModel>();
    LogComponentEnable("WbanErrorModelPlot", LOG_LEVEL_DEBUG);

    double increment = 0.1;
    double minSnr = 0; // dB
    double maxSnr = 16;

    Gnuplot berplot = Gnuplot("ber.eps");
    Gnuplot2dDataset uncodedberdataset("Un-coded  DBPSK");
    Gnuplot2dDataset codedberdataset("Coded  DBPSK (63,51,2)");
    Gnuplot2dDataset codedberdataset31("Coded  DBPSK (31,19,2)");

    for (double snr = minSnr; snr <= maxSnr; snr += increment)
    {
        double ber = ((1.0 / 2.0) * exp(-pow(10.0, snr / 10.0) * (51.0 / 63.0)));
        NS_LOG_DEBUG(snr << "(dB) " << ber << " (BER)");
        codedberdataset.Add(snr, ber);
    }

    for (double snr = minSnr; snr <= maxSnr; snr += increment)
    {
        double ber = ((1.0 / 2.0) * exp(-pow(10.0, snr / 10.0) * (19.0 / 31.0)));
        NS_LOG_DEBUG(snr << "(dB) " << ber << " (BER)");
        codedberdataset31.Add(snr, ber);
    }

    for (double snr = minSnr; snr <= maxSnr; snr += increment)
    {
        double ber = ((1.0 / 2.0) * exp(-pow(10.0, snr / 10.0)));

        NS_LOG_DEBUG(snr << "(dB) " << ber << " (BER)");
        uncodedberdataset.Add(snr, ber);
    }

    berplot.AddDataset(codedberdataset);
    berplot.AddDataset(codedberdataset31);
    berplot.AddDataset(uncodedberdataset);

    berplot.SetTerminal("postscript eps color enh \"Times-BoldItalic\"");
    berplot.SetLegend("Eb/N0 (dB)", "Bit Error Rate (BER)");
    berplot.SetExtra("set xrange [0:16]\n\
set yrange [1e-8:1]\n\
set datafile separator "
                     "\n\
set log y\n\
set grid\n\
set style line 1 linewidth 5\n\
set style line 2 linewidth 5\n\
set style line 3 linewidth 5\n\
set style line 4 linewidth 5\n\
set style line 5 linewidth 5\n\
set style line 6 linewidth 5\n\
set style line 7 linewidth 5\n\
set style line 8 linewidth 5\n\
set style increment user");
    berplot.GenerateOutput(berfile);
    berfile.close();

    return 0;
}
