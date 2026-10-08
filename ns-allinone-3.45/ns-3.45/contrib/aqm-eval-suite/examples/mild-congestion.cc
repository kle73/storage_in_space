/*
 * Copyright (c) 2017 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 *
 *
 * Authors: Ankit Deepak <adadeepak8@gmail.com>
 *          Shravya K. S. <shravya.ks0@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

/*
 * This example is equivalent to the scenario described in Section 8.2.2
 * of RFC 7928 (https://tools.ietf.org/html/rfc7928#section-8.2.2).
 */

#include "ns3/aqm-eval-suite-module.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MildCongestion");

/**
 * @brief MildCongestion class
 */
class MildCongestion : public ScenarioImpl
{
  public:
    MildCongestion();
    ~MildCongestion() override;

  protected:
    EvaluationTopology CreateScenario(std::string aqm, bool isBql) override;
};

MildCongestion::MildCongestion()
{
}

MildCongestion::~MildCongestion()
{
}

EvaluationTopology
MildCongestion::CreateScenario(std::string aqm, bool isBql)
{
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("45ms"));
    uint32_t nflow = 0.036 * 62;

    std::string baseOutputDir = GetBaseOutputDir();

    EvaluationTopology et("MildCongestion", nflow, pointToPoint, aqm, 698, isBql, baseOutputDir);
    for (uint32_t i = 0; i < nflow; i++)
    {
        ApplicationContainer ac = et.CreateFlow(StringValue("1ms"),
                                                StringValue("1ms"),
                                                StringValue("10Mbps"),
                                                StringValue("10Mbps"),
                                                "ns3::TcpNewReno",
                                                0,
                                                DataRate("10Mb/s"),
                                                3);

        ac.Start(Seconds((i * nflow) / nflow));
        ac.Stop(Seconds(300 + (i * nflow) / nflow));
    }
    return et;
}

int
main(int argc, char* argv[])
{
    std::string QueueDiscMode = "";
    std::string isBql = "";
    std::string BaseOutputDir = "aqm-eval-output";
    bool ecn = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("QueueDiscMode", "Determines the unit for QueueLimit", QueueDiscMode);
    cmd.AddValue("isBql", "Enables/Disables Byte Queue Limits", isBql);
    cmd.AddValue("BaseOutputDir", "Base output directory for results", BaseOutputDir);
    cmd.AddValue("ecn", "Enables/Disables ECN", ecn);
    cmd.Parse(argc, argv);

    MildCongestion sce;
    sce.SetBaseOutputDir(BaseOutputDir); // Set the base output directory using dependency injection
    sce.ConfigureQueueDisc(45, 750, "1Mbps", "48ms", QueueDiscMode, ecn);
    sce.RunSimulation(Seconds(310), isBql == "true");
}
