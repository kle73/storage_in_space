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
 * This example is equivalent to the scenario described in Section 6.2
 * of RFC 7928 (https://tools.ietf.org/html/rfc7928#section-6.2).
 */

#include "vector"

#include "ns3/aqm-eval-suite-module.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RttFairness");

/**
 * @brief RttFairness class
 */
class RttFairness : public ScenarioImpl
{
  public:
    /**
     * @brief Constructor
     *
     * @param run The run number for the scenario
     */
    RttFairness(uint32_t run);
    ~RttFairness() override;

  protected:
    EvaluationTopology CreateScenario(std::string aqm, bool isBql) override;

  private:
    /**
     * @brief Run number for the scenario
     */
    uint32_t m_run;

    /**
     * @brief Delay values for different runs
     */
    std::vector<uint32_t> delay;
};

RttFairness::RttFairness(uint32_t run)
{
    m_run = run;
    delay = {5, 8, 10, 16, 20, 30, 40, 60, 80, 100, 150, 200, 300, 400, 500};
}

RttFairness::~RttFairness()
{
}

EvaluationTopology
RttFairness::CreateScenario(std::string aqm, bool isBql)
{
    double bottleneck;
    double reqDelayConstRtt;
    double reqDelay;
    char OWD[20];
    char scenario[20];
    char OWDConst[20];
    char bottleneckStr[20];
    if (delay[m_run] < 100)
    {
        bottleneck = delay[m_run] * 0.8 / 2;
        reqDelay = delay[m_run] * 0.2 / 4;
        reqDelayConstRtt = (50 - bottleneck) / 2;
    }
    else
    {
        bottleneck = 40;
        reqDelay = (delay[m_run] - 80) / 4;
        reqDelayConstRtt = 5;
    }

    snprintf(OWD, sizeof(OWD), "%fms", reqDelay);
    snprintf(OWDConst, sizeof(OWDConst), "%fms", reqDelayConstRtt);
    snprintf(scenario, sizeof(scenario), "%d", m_run + 1);
    snprintf(bottleneckStr, sizeof(bottleneckStr), "%fms", bottleneck);
    std::string scenarioName = std::string("RttFairness") + std::string(scenario);
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue(bottleneckStr));
    uint32_t nflow = 2;

    std::string baseOutputDir = GetBaseOutputDir();

    EvaluationTopology et(scenarioName, nflow, pointToPoint, aqm, 698, isBql, baseOutputDir);
    ApplicationContainer ac1 = et.CreateFlow(StringValue(OWDConst),
                                             StringValue(OWDConst),
                                             StringValue("10Mbps"),
                                             StringValue("10Mbps"),
                                             "ns3::TcpNewReno",
                                             0,
                                             DataRate("10Mb/s"),
                                             3);

    ApplicationContainer ac2 = et.CreateFlow(StringValue(OWD),
                                             StringValue(OWD),
                                             StringValue("10Mbps"),
                                             StringValue("10Mbps"),
                                             "ns3::TcpNewReno",
                                             0,
                                             DataRate("10Mb/s"),
                                             3);

    ac1.Start(Seconds(0));
    ac1.Stop(Seconds(600));

    ac2.Start(Seconds(0.5));
    ac2.Stop(Seconds(600.5));
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

    for (uint32_t i = 0; i < 15; i++)
    {
        RttFairness rf(i);
        rf.SetBaseOutputDir(BaseOutputDir);
        rf.ConfigureQueueDisc(45, 750, "1Mbps", "2ms", QueueDiscMode, ecn);
        rf.RunSimulation(Seconds(610), isBql == "true");
    }
}
