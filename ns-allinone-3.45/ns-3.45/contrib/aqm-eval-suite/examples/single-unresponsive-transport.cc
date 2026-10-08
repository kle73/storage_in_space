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
 * This example is equivalent to the first scenario described in Section 5.3
 * of RFC 7928 (https://tools.ietf.org/html/rfc7928#section-5.3).
 */

#include "ns3/aqm-eval-suite-module.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SingleUnresponsiveTransportSender");

/**
 * @brief UnresponsiveTransport class
 *
 * This class implements a scenario where a single unresponsive UDP flow is evaluated under
 * different AQM algorithms.
 */
class UnresponsiveTransport : public ScenarioImpl
{
  public:
    UnresponsiveTransport();
    ~UnresponsiveTransport() override;

  protected:
    EvaluationTopology CreateScenario(std::string aqm, bool isBql) override;
};

UnresponsiveTransport::UnresponsiveTransport()
{
}

UnresponsiveTransport::~UnresponsiveTransport()
{
}

EvaluationTopology
UnresponsiveTransport::CreateScenario(std::string aqm, bool isBql)
{
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("48ms"));
    uint32_t nflow = 1;

    std::string baseOutputDir = GetBaseOutputDir();

    EvaluationTopology
        et("UnresponsiveTransport", nflow, pointToPoint, aqm, 698, isBql, baseOutputDir);
    ApplicationContainer ac = et.CreateFlow(StringValue("1ms"),
                                            StringValue("1ms"),
                                            StringValue("10Mbps"),
                                            StringValue("10Mbps"),
                                            "udp",
                                            0,
                                            DataRate("10Mb/s"));

    ac.Start(Seconds(0));
    ac.Stop(Seconds(300));
    return et;
}

int
main(int argc, char* argv[])
{
    // ECN is disabled for UDP scenarios since UDP doesn't respond to congestion signals

    std::string QueueDiscMode = "";
    std::string isBql = "";
    std::string BaseOutputDir = "aqm-eval-output";

    CommandLine cmd(__FILE__);
    cmd.AddValue("QueueDiscMode", "Determines the unit for QueueLimit", QueueDiscMode);
    cmd.AddValue("isBql", "Enables/Disables Byte Queue Limits", isBql);
    cmd.AddValue("BaseOutputDir", "Base output directory for results", BaseOutputDir);
    cmd.Parse(argc, argv);

    UnresponsiveTransport sce;
    sce.SetBaseOutputDir(BaseOutputDir);
    // ECN is always disabled for UDP flows since they are not congestion-aware
    sce.ConfigureQueueDisc(45, 750, "1Mbps", "48ms", QueueDiscMode, false);
    sce.RunSimulation(Seconds(310), isBql == "true");
}
