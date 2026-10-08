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
 * This example is equivalent to the scenario described in Section 5.2
 * of RFC 7928 (https://tools.ietf.org/html/rfc7928#section-5.2).
 *
 * Note: BIC TCP is used as an aggressive sender due to unavailability
 * of CUBIC TCP module in ns-3.
 */

#include "ns3/aqm-eval-suite-module.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

using namespace ns3;

/**
 * @brief AggressiveTransportSender class
 */
class AggressiveTransportSender : public ScenarioImpl
{
  public:
    AggressiveTransportSender();
    ~AggressiveTransportSender() override;

  protected:
    EvaluationTopology CreateScenario(std::string aqm, bool isBql) override;
};

std::string AggressiveTcpVariant = "ns3::TcpCubic";

AggressiveTransportSender::AggressiveTransportSender()
{
}

AggressiveTransportSender::~AggressiveTransportSender()
{
}

EvaluationTopology
AggressiveTransportSender::CreateScenario(std::string aqm, bool isBql)
{
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("48ms"));
    uint32_t nflow = 1;

    std::string baseOutputDir = GetBaseOutputDir();

    EvaluationTopology
        et("AggressiveTransportSender", nflow, pointToPoint, aqm, 698, isBql, baseOutputDir);
    ApplicationContainer ac = et.CreateFlow(StringValue("1ms"),
                                            StringValue("1ms"),
                                            StringValue("10Mbps"),
                                            StringValue("10Mbps"),
                                            AggressiveTcpVariant,
                                            0,
                                            DataRate("10Mb/s"),
                                            3);

    ac.Start(Seconds(0));
    ac.Stop(Seconds(300));
    return et;
}

int
main(int argc, char* argv[])
{
    Config::SetDefault("ns3::TcpSocketState::EnablePacing", BooleanValue(true));
    Config::SetDefault("ns3::TcpSocketState::PaceInitialWindow", BooleanValue(true));

    std::string QueueDiscMode = "";
    std::string TcpVariant = "";
    std::string isBql = "";
    std::string BaseOutputDir = "aqm-eval-output";
    bool ecn = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("QueueDiscMode", "Determines the unit for QueueLimit", QueueDiscMode);
    cmd.AddValue("TcpVariant", "Aggressive TCP variant", TcpVariant);
    cmd.AddValue("isBql", "Enables/Disables Byte Queue Limits", isBql);
    cmd.AddValue("BaseOutputDir", "Base output directory for results", BaseOutputDir);
    cmd.AddValue("ecn", "Enables/Disables ECN", ecn);

    cmd.Parse(argc, argv);
    if ((TcpVariant == "ns3::TcpYeah") || (TcpVariant == "ns3::TcpBic") ||
        (TcpVariant == "ns3::TcpHighSpeed") || (TcpVariant == "ns3::TcpIllinois") ||
        (TcpVariant == "ns3::TcpScalable") || (TcpVariant == "ns3::TcpHtcp") ||
        (TcpVariant == "ns3::TcpCubic"))
    {
        AggressiveTcpVariant = TcpVariant;
    }
    AggressiveTransportSender sce;
    sce.SetBaseOutputDir(BaseOutputDir);
    sce.ConfigureQueueDisc(45, 750, "1Mbps", "48ms", QueueDiscMode, ecn);
    sce.RunSimulation(Seconds(310), isBql == "true");
}
