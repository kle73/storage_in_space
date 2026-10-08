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
 * This example is equivalent to the scenario described in Section 5.1.2
 * of RFC 7928 (https://tools.ietf.org/html/rfc7928#section-5.1).
 */

#include "ns3/aqm-eval-suite-module.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LbeTransportSender");

/**
 * @brief LbeTransportSender class
 */
class LbeTransportSender : public ScenarioImpl
{
  public:
    LbeTransportSender();
    ~LbeTransportSender() override;

  protected:
    EvaluationTopology CreateScenario(std::string aqm, bool isBql) override;
};

LbeTransportSender::LbeTransportSender()
{
}

LbeTransportSender::~LbeTransportSender()
{
}

EvaluationTopology
LbeTransportSender::CreateScenario(std::string aqm, bool isBql)
{
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("48ms"));
    uint32_t nflow = 2;

    std::string baseOutputDir = GetBaseOutputDir();

    EvaluationTopology
        et("LbeTransportSender", nflow, pointToPoint, aqm, 698, isBql, baseOutputDir);
    ApplicationContainer ac1 = et.CreateFlow(StringValue("1ms"),
                                             StringValue("1ms"),
                                             StringValue("10Mbps"),
                                             StringValue("10Mbps"),
                                             "ns3::TcpLedbat",
                                             0,
                                             DataRate("10Mb/s"),
                                             3);

    ApplicationContainer ac2 = et.CreateFlow(StringValue("1ms"),
                                             StringValue("1ms"),
                                             StringValue("10Mbps"),
                                             StringValue("10Mbps"),
                                             "ns3::TcpNewReno",
                                             0,
                                             DataRate("10Mb/s"),
                                             3);

    ac1.Start(Seconds(0));
    ac1.Stop(Seconds(300));

    ac2.Start(Seconds(0));
    ac2.Stop(Seconds(300));
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

    LbeTransportSender sce;
    sce.SetBaseOutputDir(BaseOutputDir);
    sce.ConfigureQueueDisc(45, 750, "1Mbps", "48ms", QueueDiscMode, ecn);
    sce.RunSimulation(Seconds(310), isBql == "true");
}
