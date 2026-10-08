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

#include "aqm-eval-suite-helper.h"

namespace ns3
{

ScenarioImpl::ScenarioImpl()
{
    m_AQM = {"ns3::PfifoFastQueueDisc",
             "ns3::RedQueueDisc",
             "ns3::AdaptiveRedQueueDisc",
             "ns3::FengAdaptiveRedQueueDisc",
             "ns3::NonLinearRedQueueDisc",
             "ns3::CoDelQueueDisc",
             "ns3::FqCoDelQueueDisc",
             "ns3::PieQueueDisc",
             "ns3::FqPieQueueDisc",
             "ns3::CobaltQueueDisc",
             "ns3::FqCobaltQueueDisc"};
    m_nAQM = 11;
    m_baseOutputDir = "aqm-eval-output";
}

ScenarioImpl::~ScenarioImpl()
{
}

void
ScenarioImpl::addAQM(std::string aqm)
{
    m_AQM.push_back(aqm);
    m_nAQM++;
}

void
ScenarioImpl::DestroyTrace(EvaluationTopology et)
{
    et.DestroyConnection();
}

void
ScenarioImpl::ConfigureQueueDisc(uint32_t limit,
                                 uint32_t pktsize,
                                 std::string linkbw,
                                 std::string linkdel,
                                 std::string mode,
                                 bool ecn)
{
    Config::SetDefault("ns3::PfifoFastQueueDisc::MaxSize",
                       StringValue(std::to_string(limit) + "p"));

    Config::SetDefault("ns3::RedQueueDisc::MeanPktSize", UintegerValue(pktsize));
    Config::SetDefault("ns3::RedQueueDisc::LinkBandwidth", StringValue(linkbw));
    Config::SetDefault("ns3::RedQueueDisc::LinkDelay", StringValue(linkdel));

    Config::SetDefault("ns3::CoDelQueueDisc::MaxSize", StringValue(std::to_string(limit) + "p"));
    Config::SetDefault("ns3::CoDelQueueDisc::MinBytes", UintegerValue(pktsize));

    Config::SetDefault("ns3::PieQueueDisc::MeanPktSize", UintegerValue(pktsize));

    if (mode == "QUEUE_DISC_MODE_PACKETS")
    {
        Config::SetDefault("ns3::RedQueueDisc::MaxSize", StringValue(std::to_string(limit) + "p"));
        Config::SetDefault("ns3::RedQueueDisc::MinTh", DoubleValue(5));
        Config::SetDefault("ns3::RedQueueDisc::MaxTh", DoubleValue(15));

        Config::SetDefault("ns3::PieQueueDisc::MaxSize", StringValue(std::to_string(limit) + "p"));
    }
    else
    {
        Config::SetDefault("ns3::RedQueueDisc::MaxSize",
                           StringValue(std::to_string(limit * pktsize) + "B"));
        Config::SetDefault("ns3::RedQueueDisc::MinTh", DoubleValue(5 * pktsize));
        Config::SetDefault("ns3::RedQueueDisc::MaxTh", DoubleValue(15 * pktsize));

        Config::SetDefault("ns3::PieQueueDisc::MaxSize",
                           StringValue(std::to_string(limit * pktsize) + "B"));
    }

    if (ecn)
    {
        Config::SetDefault("ns3::TcpSocketBase::UseEcn", StringValue("On"));

        Config::SetDefault("ns3::RedQueueDisc::UseEcn", BooleanValue(true));
        Config::SetDefault("ns3::CoDelQueueDisc::UseEcn", BooleanValue(true));
        Config::SetDefault("ns3::FqCoDelQueueDisc::UseEcn", BooleanValue(true));
        Config::SetDefault("ns3::PieQueueDisc::UseEcn", BooleanValue(true));
        Config::SetDefault("ns3::FqPieQueueDisc::UseEcn", BooleanValue(true));
        Config::SetDefault("ns3::CobaltQueueDisc::UseEcn", BooleanValue(true));
        Config::SetDefault("ns3::FqCobaltQueueDisc::UseEcn", BooleanValue(true));
    }
}

void
ScenarioImpl::SetBaseOutputDir(const std::string& baseOutputDir)
{
    m_baseOutputDir = baseOutputDir;
}

std::string
ScenarioImpl::GetBaseOutputDir() const
{
    return m_baseOutputDir;
}

void
ScenarioImpl::RunSimulation(Time simtime, bool isBql)
{
    for (uint32_t i = 0; i < m_nAQM; i++)
    {
        EvaluationTopology et = CreateScenario(m_AQM[i], isBql);
        Simulator::Schedule(simtime, &ScenarioImpl::DestroyTrace, this, et);
        Simulator::Stop(simtime);
        Simulator::Run();
        Simulator::Destroy();
    }
}

} // namespace ns3
