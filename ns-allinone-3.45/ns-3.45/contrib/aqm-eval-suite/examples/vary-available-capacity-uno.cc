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
 * This example is equivalent to the scenario described in Experiment 1 of
 * Section 8.2.6 of RFC 7928 (https://tools.ietf.org/html/rfc7928#section-8.2.6).
 */

#include "ns3/aqm-eval-suite-module.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VaryAvailableCapacityUno");

/**
 * @brief VaryingBandwidthUno class
 */
class VaryingBandwidthUno : public ScenarioImpl
{
  public:
    VaryingBandwidthUno();
    ~VaryingBandwidthUno() override;

  protected:
    EvaluationTopology CreateScenario(std::string aqm, bool isBql) override;

  private:
    /**
     * @brief Changes the data rate of the links
     */
    void ChangeDataRate();

    /**
     * @brief The state of the data rate, true for 100Mbps and false for 10Mbps
     */
    bool m_state;
};

VaryingBandwidthUno::VaryingBandwidthUno()
{
    m_state = true;
}

VaryingBandwidthUno::~VaryingBandwidthUno()
{
}

void
VaryingBandwidthUno::ChangeDataRate()
{
    if (m_state)
    {
        Config::Set("/NodeList/0/DeviceList/0/DataRate", StringValue("10Mbps"));
        Config::Set("/NodeList/1/DeviceList/0/DataRate", StringValue("10Mbps"));
        m_state = false;
    }
    else
    {
        Config::Set("/NodeList/0/DeviceList/0/DataRate", StringValue("100Mbps"));
        Config::Set("/NodeList/1/DeviceList/0/DataRate", StringValue("100Mbps"));
        m_state = true;
    }
}

EvaluationTopology
VaryingBandwidthUno::CreateScenario(std::string aqm, bool isBql)
{
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("48ms"));
    uint32_t nflow = 2;

    std::string baseOutputDir = GetBaseOutputDir();

    EvaluationTopology
        et("VaryingBandwidthUno", nflow, pointToPoint, aqm, 698, isBql, baseOutputDir);
    ApplicationContainer ac1 = et.CreateFlow(StringValue("1ms"),
                                             StringValue("1ms"),
                                             StringValue("500Mbps"),
                                             StringValue("500Mbps"),
                                             "ns3::TcpNewReno",
                                             0,
                                             DataRate("500Mb/s"),
                                             3);

    ApplicationContainer ac2 = et.CreateFlow(StringValue("1ms"),
                                             StringValue("1ms"),
                                             StringValue("500Mbps"),
                                             StringValue("500Mbps"),
                                             "ns3::TcpNewReno",
                                             0,
                                             DataRate("500Mb/s"),
                                             3);

    ac1.Start(Seconds(0));
    ac1.Stop(Seconds(300));

    ac2.Start(Seconds(0));
    ac2.Stop(Seconds(300));

    for (double i = 20; i < 300; i += 20)
    {
        Simulator::Schedule(Seconds(i), &VaryingBandwidthUno::ChangeDataRate, this);
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

    VaryingBandwidthUno sce;
    sce.SetBaseOutputDir(BaseOutputDir); // Set the base output directory using dependency injection
    sce.ConfigureQueueDisc(500, 750, "100Mbps", "48ms", QueueDiscMode, ecn);
    sce.RunSimulation(Seconds(310), isBql == "true");
}
