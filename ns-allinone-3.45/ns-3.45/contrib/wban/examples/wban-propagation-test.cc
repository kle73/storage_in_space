/*
 * Copyright (c) 2011 The Boeing Company
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Drishti Oza
 */

/*
 * This program is a comparison with existing study mentioned in "International conference
 * on Ns3 2025" paper
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
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace ns3;
using namespace std;
using namespace ns3::wban;
NS_LOG_COMPONENT_DEFINE("WbanPerModelPlot");

//
// Plot 802.15.6 BER curve
//
int
main(int argc, char* argv[])
{
    std::ofstream propagationfile("propagation.plt");
    Ptr<WbanErrorModel> WbanError = CreateObject<WbanErrorModel>();

    double increment = 0.01;
    double minDis = 0; // dB
    double maxDis = 1.1;

    Gnuplot proplot = Gnuplot("propagation.eps");

    Gnuplot2dDataset datasetskin24("skin 2.4 GHz");
    Gnuplot2dDataset datasetfat24("fat 2.4 GHz");
    Gnuplot2dDataset datasetmuscle24("muscle 2.4 GHz");

    for (double dis = minDis; dis <= maxDis; dis += increment)
    {
        double testskinalpha24 = ((521 * M_PI * 1.44079) / pow(38.06, 0.5)) * dis;
        datasetskin24.Add(dis, testskinalpha24);

        double testfatalpha24 = ((521 * M_PI * 0.1) / pow(5.28, 0.5)) * dis;
        datasetfat24.Add(dis, testfatalpha24);

        double testmusclealpha24 = ((521 * M_PI * 1.705) / pow(50, 0.5)) * dis;
        datasetmuscle24.Add(dis, testmusclealpha24);
    }
    proplot.AddDataset(datasetskin24);
    proplot.AddDataset(datasetfat24);
    proplot.AddDataset(datasetmuscle24);

    proplot.SetTerminal("postscript eps color enh \"Times-BoldItalic\"");
    proplot.SetLegend("(m)", "attenuation");
    proplot.SetExtra("set xrange [0.1:1]\n\
                      set yrange [1:400]\n\
                      set grid\n\
set key reverse top left\n\
set linetype 1 linewidth 9 lc rgb 'blue'\n\
set linetype 2 linewidth 9 lc rgb 'green'\n\
set linetype 3 linewidth 9 lc rgb 'red'\n\
");
    proplot.GenerateOutput(propagationfile);
    propagationfile.close();

    return 0;
}
