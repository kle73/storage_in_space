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

#include "eval-topology.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("EvaluationTopology");

NS_OBJECT_ENSURE_REGISTERED(EvaluationTopology);

TypeId
EvaluationTopology::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::EvaluationTopology").SetParent<Object>().SetGroupName("AqmEvaluationSuite");
    return tid;
}

EvaluationTopology::EvaluationTopology(std::string ScenarioName,
                                       uint32_t numFlows,
                                       PointToPointHelper p2pHelper,
                                       std::string queueDisc,
                                       uint32_t pktSize,
                                       bool isBql,
                                       std::string baseOutputDir)
    : m_dumbbell(numFlows, p2pHelper, numFlows, p2pHelper, p2pHelper)
{
    m_numFlows = numFlows;
    m_flowsAdded = 0;
    m_packetSize = pktSize;
    bool m_isBql = isBql;

    InternetStackHelper stack;
    m_dumbbell.InstallStack(stack);

    TrafficControlHelper tch;
    m_dumbbell.AssignIpv4Addresses(Ipv4AddressHelper("10.1.1.0", "255.255.255.0"),
                                   Ipv4AddressHelper("10.10.1.0", "255.255.255.0"),
                                   Ipv4AddressHelper("10.100.1.0", "255.255.255.0"));
    tch.Uninstall(m_dumbbell.GetLeft()->GetDevice(0));

    if (m_isBql)
    {
        tch.SetQueueLimits("ns3::DynamicQueueLimits");
    }

    m_currentAQM = queueDisc;
    if (queueDisc == "ns3::AdaptiveRedQueueDisc" || queueDisc == "ns3::FengAdaptiveRedQueueDisc" ||
        queueDisc == "ns3::NonLinearRedQueueDisc")
    {
        queueDisc = "ns3::RedQueueDisc";
        tch.SetRootQueueDisc(queueDisc);
        m_queue = tch.Install(m_dumbbell.GetLeft()->GetDevice(0)).Get(0);
        if (m_currentAQM == "ns3::AdaptiveRedQueueDisc")
        {
            m_queue->SetAttribute("ARED", BooleanValue(true));
        }
        else if (m_currentAQM == "ns3::FengAdaptiveRedQueueDisc")
        {
            m_queue->SetAttribute("FengAdaptive", BooleanValue(true));
        }
        else if (m_currentAQM == "ns3::NonLinearRedQueueDisc")
        {
            m_queue->SetAttribute("NLRED", BooleanValue(true));
        }
    }
    else
    {
        tch.SetRootQueueDisc(queueDisc);
        m_queue = tch.Install(m_dumbbell.GetLeft()->GetDevice(0)).Get(0);
    }

    if (queueDisc == "ns3::RedQueueDisc")
    {
        StaticCast<RedQueueDisc>(m_queue)->AssignStreams(0);
    }
    else if (queueDisc == "ns3::PieQueueDisc")
    {
        StaticCast<PieQueueDisc>(m_queue)->AssignStreams(0);
    }
    m_queue->TraceConnectWithoutContext("Enqueue",
                                        MakeCallback(&EvaluationTopology::PacketEnqueue, this));
    m_queue->TraceConnectWithoutContext("Dequeue",
                                        MakeCallback(&EvaluationTopology::PacketDequeue, this));
    m_queue->TraceConnectWithoutContext("Drop",
                                        MakeCallback(&EvaluationTopology::PacketDrop, this));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    m_QDrecord = 0;
    m_numQDrecord = 0;
    m_lastQDrecord = Time::Min();
    m_currentAQM.replace(m_currentAQM.begin(), m_currentAQM.begin() + 5, "/");
    AsciiTraceHelper asciiQD;
    std::string default_directory = baseOutputDir + "/";
    std::string data = "/data";
    m_QDfile = asciiQD.CreateFileStream(
        std::string(default_directory + ScenarioName + data + m_currentAQM + "-qdel.dat"));
    m_TPrecord = 0;
    m_lastTPrecord = Time::Min();
    AsciiTraceHelper asciiTP;
    m_TPfile = asciiTP.CreateFileStream(
        std::string(default_directory + ScenarioName + data + m_currentAQM + "-throughput.dat"));
    AsciiTraceHelper asciiGP;
    m_GPfile = asciiGP.CreateFileStream(
        std::string(default_directory + ScenarioName + data + m_currentAQM + "-goodput.dat"));
    AsciiTraceHelper asciiMD;
    m_metaData = asciiMD.CreateFileStream(
        std::string(default_directory + ScenarioName + data + m_currentAQM + "-metadata.dat"));
    AsciiTraceHelper asciiDT;
    m_dropTime = asciiDT.CreateFileStream(
        std::string(default_directory + ScenarioName + data + m_currentAQM + "-drop.dat"));
    AsciiTraceHelper asciiET;
    m_enqueueTime = asciiET.CreateFileStream(
        std::string(default_directory + ScenarioName + data + m_currentAQM + "-enqueue.dat"));
}

EvaluationTopology::~EvaluationTopology()
{
}

void
EvaluationTopology::DestroyConnection()
{
    m_queue->TraceDisconnectWithoutContext("Enqueue",
                                           MakeCallback(&EvaluationTopology::PacketEnqueue, this));
    m_queue->TraceDisconnectWithoutContext("Dequeue",
                                           MakeCallback(&EvaluationTopology::PacketDequeue, this));
    m_queue->TraceDisconnectWithoutContext("Drop",
                                           MakeCallback(&EvaluationTopology::PacketDrop, this));

    for (uint32_t i = 0; i < m_sinks.size(); i++)
    {
        m_sinks[i]->TraceDisconnectWithoutContext(
            "Rx",
            MakeCallback(&EvaluationTopology::PayloadSize, this));
    }
    for (uint32_t i = 0; i < m_sources.size(); i++)
    {
        *m_metaData->GetStream() << "The flow completion time of flow " << (i + 1) << " = "
                                 << (m_sources[i]->GetFlowCompletionTime()).GetSeconds() << "\n";
    }
}

void
EvaluationTopology::PacketEnqueue(Ptr<const QueueDiscItem> item)
{
    const_cast<QueueDiscItem*>(PeekPointer(item))->SetTimeStamp(Simulator::Now());

    Ptr<Packet> p = item->GetPacket();
    Ptr<const Ipv4QueueDiscItem> iqdi =
        Ptr<const Ipv4QueueDiscItem>(dynamic_cast<const Ipv4QueueDiscItem*>(PeekPointer(item)));
    *m_enqueueTime->GetStream() << (iqdi->GetHeader()).GetDestination() << " "
                                << Simulator::Now().GetSeconds() << "\n";
}

void
EvaluationTopology::PacketDequeue(Ptr<const QueueDiscItem> item)
{
    Time delta = Simulator::Now() - item->GetTimeStamp();

    Ptr<Packet> p = item->GetPacket();
    if (m_lastQDrecord == Time::Min() || Simulator::Now() - m_lastQDrecord > MilliSeconds(10))
    {
        m_lastQDrecord = Simulator::Now();
        if (m_numQDrecord > 0)
        {
            *m_QDfile->GetStream() << Simulator::Now().GetSeconds() << " "
                                   << (m_QDrecord * 1.0) / (m_numQDrecord * 1.0) << "\n";
        }
        m_QDrecord = 0;
        m_numQDrecord = 0;
    }
    m_numQDrecord++;
    m_QDrecord += delta.GetMilliSeconds();
}

void
EvaluationTopology::PacketDrop(Ptr<const QueueDiscItem> item)
{
    Ptr<const Ipv4QueueDiscItem> iqdi =
        Ptr<const Ipv4QueueDiscItem>(dynamic_cast<const Ipv4QueueDiscItem*>(PeekPointer(item)));
    *m_dropTime->GetStream() << (iqdi->GetHeader()).GetDestination() << " "
                             << Simulator::Now().GetSeconds() << "\n";
}

void
EvaluationTopology::PayloadSize(Ptr<const Packet> packet, const Address& address)
{
    *m_GPfile->GetStream() << address << " " << Simulator::Now().GetSeconds() << " "
                           << packet->GetSize() << "\n";
    if (m_lastTPrecord == Time::Min() || Simulator::Now() - m_lastTPrecord > MilliSeconds(10))
    {
        if (m_TPrecord > 0)
        {
            *m_TPfile->GetStream()
                << Simulator::Now().GetSeconds() << " "
                << (m_TPrecord * 1.0) / (Simulator::Now() - m_lastTPrecord).GetSeconds() << "\n";
        }
        m_lastTPrecord = Simulator::Now();
        m_TPrecord = 0;
    }
    m_TPrecord += packet->GetSize();
}

ApplicationContainer
EvaluationTopology::CreateFlow(StringValue senderDelay,
                               StringValue receiverDelay,
                               StringValue senderBW,
                               StringValue receiverBW,
                               std::string transport_prot,
                               uint64_t maxPacket,
                               DataRate rate,
                               uint32_t initCwnd)
{
    NS_ASSERT_MSG(m_flowsAdded < m_numFlows, "Trying to create more flows than permitted");
    m_flowsAdded++;

    char tempstr[20];

    snprintf(tempstr, sizeof(tempstr), "%d", m_flowsAdded);
    std::string sdelayAddress = std::string("/ChannelList/") + tempstr + std::string("/Delay");
    std::string rrBWAddress =
        std::string("/NodeList/1/DeviceList/") + tempstr + std::string("/DataRate");
    std::string srBWAddress =
        std::string("/NodeList/0/DeviceList/") + tempstr + std::string("/DataRate");

    snprintf(tempstr, sizeof(tempstr), "%d", m_flowsAdded + 1);
    std::string slBWAddress =
        std::string("/NodeList/") + tempstr + std::string("/DeviceList/0/DataRate");
    std::string socketTypeAddress =
        std::string("/NodeList/") + tempstr + std::string("/$ns3::TcpL4Protocol/SocketType");

    snprintf(tempstr, sizeof(tempstr), "%d", m_numFlows + m_flowsAdded);
    std::string rdelayAddress = std::string("/ChannelList/") + tempstr + std::string("/Delay");

    snprintf(tempstr, sizeof(tempstr), "%d", m_numFlows + m_flowsAdded + 1);
    std::string rlBWAddress =
        std::string("/NodeList/") + tempstr + std::string("/DeviceList/0/DataRate");

    Config::Set(sdelayAddress, senderDelay);
    Config::Set(rdelayAddress, receiverDelay);
    Config::Set(slBWAddress, senderBW);
    Config::Set(srBWAddress, senderBW);
    Config::Set(rlBWAddress, receiverBW);
    Config::Set(rrBWAddress, receiverBW);

    if (transport_prot == "udp")
    {
        uint32_t port = 50000;
        AddressValue remoteAddress(
            InetSocketAddress(m_dumbbell.GetRightIpv4Address(m_flowsAdded - 1), port));
        ApplicationContainer sourceAndSinkApp;
        PacketSinkHelper sinkHelper(
            "ns3::UdpSocketFactory",
            InetSocketAddress(m_dumbbell.GetRightIpv4Address(m_flowsAdded - 1), port));
        sinkHelper.SetAttribute("Protocol", TypeIdValue(UdpSocketFactory::GetTypeId()));

        Ptr<Socket> ns3UdpSocket = Socket::CreateSocket(m_dumbbell.GetLeft(m_flowsAdded - 1),
                                                        UdpSocketFactory::GetTypeId());

        Ptr<EvalApp> app = CreateObject<EvalApp>();
        app->Setup(ns3UdpSocket,
                   remoteAddress.Get(),
                   m_packetSize,
                   maxPacket * m_packetSize,
                   m_flowsAdded * 2,
                   rate);
        m_dumbbell.GetLeft(m_flowsAdded - 1)->AddApplication(app);

        sourceAndSinkApp.Add(app);
        sourceAndSinkApp.Add(sinkHelper.Install(m_dumbbell.GetRight(m_flowsAdded - 1)));
        Ptr<Application> appSink = sourceAndSinkApp.Get(1);
        Ptr<PacketSink> psink = Ptr<PacketSink>(dynamic_cast<PacketSink*>(PeekPointer(appSink)));
        psink->TraceConnectWithoutContext("Rx",
                                          MakeCallback(&EvaluationTopology::PayloadSize, this));
        return sourceAndSinkApp;
    }
    else if (transport_prot == "ns3::TcpWestwoodPlus")
    {
        Config::Set(socketTypeAddress, TypeIdValue(TcpWestwoodPlus::GetTypeId()));
        Config::Set("ns3::TcpWestwoodPlus::FilterType", EnumValue(TcpWestwoodPlus::TUSTIN));
    }
    else
    {
        Config::Set(socketTypeAddress, TypeIdValue(TypeId::LookupByName(transport_prot)));
    }

    uint32_t port = 50000;
    AddressValue remoteAddress(
        InetSocketAddress(m_dumbbell.GetRightIpv4Address(m_flowsAdded - 1), port));
    ApplicationContainer sourceAndSinkApp;
    PacketSinkHelper sinkHelper(
        "ns3::TcpSocketFactory",
        InetSocketAddress(m_dumbbell.GetRightIpv4Address(m_flowsAdded - 1), port));
    sinkHelper.SetAttribute("Protocol", TypeIdValue(TcpSocketFactory::GetTypeId()));

    Ptr<Socket> ns3TcpSocket =
        Socket::CreateSocket(m_dumbbell.GetLeft(m_flowsAdded - 1), TcpSocketFactory::GetTypeId());
    ns3TcpSocket->SetAttribute("InitialCwnd", UintegerValue(initCwnd));
    ns3TcpSocket->SetAttribute("SegmentSize", UintegerValue(m_packetSize));

    if (transport_prot == "ns3::TcpNewReno")
    {
        ns3TcpSocket->SetAttribute("Sack", BooleanValue(true));
    }
    else
    {
        ns3TcpSocket->SetAttribute("Sack", BooleanValue(false));
    }

    Ptr<EvalApp> app = CreateObject<EvalApp>();
    app->Setup(ns3TcpSocket,
               remoteAddress.Get(),
               m_packetSize,
               maxPacket * m_packetSize,
               m_flowsAdded * 2,
               rate);
    m_dumbbell.GetLeft(m_flowsAdded - 1)->AddApplication(app);

    sourceAndSinkApp.Add(app);
    sourceAndSinkApp.Add(sinkHelper.Install(m_dumbbell.GetRight(m_flowsAdded - 1)));
    Ptr<Application> appSink = sourceAndSinkApp.Get(1);
    Ptr<PacketSink> psink = Ptr<PacketSink>(dynamic_cast<PacketSink*>(PeekPointer(appSink)));
    psink->TraceConnectWithoutContext("Rx", MakeCallback(&EvaluationTopology::PayloadSize, this));
    m_sinks.push_back(psink);
    m_sources.push_back(app);
    return sourceAndSinkApp;
}

} // namespace ns3
