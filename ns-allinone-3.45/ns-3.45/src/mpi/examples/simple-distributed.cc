/*
 * spdx-license-identifier: gpl-2.0-only
 */

/**
 * @file
 * @ingroup mpi
 *
 * testdistributed creates a dumbbell topology and logically splits it in
 * half.  the left half is placed on logical processor 0 and the right half
 * is placed on logical processor 1.
 *
 *                 -------   -------
 *                  rank 0    rank 1
 *                 ------- | -------
 *                         |
 * n0 ---------|           |           |---------- n6
 *             |           |           |
 * n1 -------\ |           |           | /------- n7
 *            n4 ----------|---------- n5
 * n2 -------/ |           |           | \------- n8
 *             |           |           |
 * n3 ---------|           |           |---------- n9
 *
 *
 * onoff clients are placed on each left leaf node. each right leaf node
 * is a packet sink for a left leaf node.  as a packet travels from one
 * logical processor to another (the link between n4 and n5), mpi messages
 * are passed containing the serialized packet. the message is then
 * deserialized into a new packet and sent on as normal.
 *
 * one packet is sent from each left leaf node.  the packet sinks on the
 * right leaf nodes output logging information when they receive the packet.
 */

#include "mpi-test-fixtures.h"

#include "ns3/core-module.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/mpi-interface.h"
#include "ns3/network-module.h"
#include "ns3/nix-vector-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

#include <iomanip>
#include <mpi.h>
#include <stdio.h>

using namespace ns3;

#define NUM_SATS_IN_ORBIT 15
#define NUM_ORBITS 6
#define NUM_SATS_PER_CORE 5

NS_LOG_COMPONENT_DEFINE("simpledistributed");

// ========================
// Custom Header
// ========================
class NHeader : public Header
{
public:
  NHeader() : m_n(0) {}
  NHeader(uint32_t n) : m_n(n) {}

  static TypeId GetTypeId()
  {
    static TypeId tid = TypeId("NHeader")
      .SetParent<Header>()
      .AddConstructor<NHeader>();
    return tid;
  }

  TypeId GetInstanceTypeId() const override { return GetTypeId(); }

  void Serialize(Buffer::Iterator start) const override
  {
    start.WriteHtonU32(m_n);
  }

  uint32_t Deserialize(Buffer::Iterator start) override
  {
    m_n = start.ReadNtohU32();
    return 4;
  }

  uint32_t GetSerializedSize() const override { return 4; }

  void Print(std::ostream &os) const override
  {
    os << "n=" << m_n;
  }

  uint32_t GetN() const { return m_n; }
  void SetN(uint32_t n) { m_n = n; }

private:
  uint32_t m_n;
};

// ========================
// Grid Node Logic
// ========================
class GridNode
{
public:
  void SetDevice(std::string dir, Ptr<NetDevice> dev)
  {
    m_devices[dir] = dev;
    dev->SetReceiveCallback(MakeCallback(&GridNode::ReceivePacket, this));
  }

  bool ReceivePacket(Ptr<NetDevice> dev,
                     Ptr<const Packet> packet,
                     uint16_t protocol,
                     const Address &from)
  {
    Ptr<Packet> p = packet->Copy();

    NHeader header;
    p->RemoveHeader(header);

    uint32_t n = header.GetN();
    std::string incoming = GetDirection(dev);

    if (n == NUM_SATS_IN_ORBIT)
    {
      n = 0;
      Send("right", n);
      return true;
    }

    n++;

    if (incoming == "down" || incoming == "left")
    {
      Send("up", n);
    }

    return true;
  }


  void Send(std::string dir, uint32_t n)
  {
    if (m_devices.find(dir) == m_devices.end())
      return;

    Ptr<Packet> p = Create<Packet>(1500);
    NHeader header(n);
    p->AddHeader(header);

    m_devices[dir]->Send(p,
                         m_devices[dir]->GetBroadcast(),
                         0x0800);
  }

  void StartGenerating()
  {
    GeneratePacket();
  }

  void GeneratePacket()
 {
  if (Simulator::Now().GetSeconds() >= 4.0)
    return;

  Send("up", 0);   

  Simulator::Schedule(MilliSeconds(1),
                      &GridNode::GeneratePacket,
                      this);
}

private:
  std::string GetDirection(Ptr<NetDevice> dev)
  {
    for (auto &entry : m_devices)
    {
      if (entry.second == dev)
        return entry.first;
    }
    return "";
  }

  std::map<std::string, Ptr<NetDevice>> m_devices;
};

void MonitorQueues(NodeContainer nodes)
{
  std::cout << "Time: " << Simulator::Now().GetSeconds() << "s\n";

  for (uint32_t i = 0; i < NUM_SATS_IN_ORBIT; ++i)
  {
    Ptr<Node> node = nodes.Get(i);

    uint64_t totalBytes = 0;

    for (uint32_t d = 0; d < node->GetNDevices(); ++d)
    {
      Ptr<PointToPointNetDevice> dev =
          node->GetDevice(d)->GetObject<PointToPointNetDevice>();

      if (dev)
      {
        Ptr<Queue<Packet>> q = dev->GetQueue();
        totalBytes += q->GetCurrentSize().GetValue();
      }
    }

    std::cout << "Node " << i
              << " total queue bytes: "
              << totalBytes << "\n";
  }

  std::cout << "----------------------\n";

  Simulator::Schedule(Seconds(0.01),
                      &MonitorQueues,
                      nodes);
}

int main(int argc, char *argv[])
{
  bool nullmsg = false;
  CommandLine cmd;
  cmd.AddValue("nullmsg", "Use null message synchronization", nullmsg);
  cmd.Parse(argc, argv);

  if (nullmsg)
  {
    GlobalValue::Bind("SimulatorImplementationType",
                      StringValue("ns3::NullMessageSimulatorImpl"));
  }
  else
  {
    GlobalValue::Bind("SimulatorImplementationType",
                      StringValue("ns3::DistributedSimulatorImpl"));
  }

  MpiInterface::Enable(&argc, &argv);

  uint32_t systemId = MpiInterface::GetSystemId();
  uint32_t systemCount = MpiInterface::GetSize();
  uint32_t num_cores_per_orbit = NUM_SATS_IN_ORBIT / NUM_SATS_PER_CORE;
  uint32_t num_cores = num_cores_per_orbit*NUM_ORBITS;

  if (systemCount != num_cores)
  {
    std::cout << "This simulation requires exactly X MPI ranks.\n";
    return 1;
  }

  NodeContainer nodes;

  for (uint32_t col = 0; col < NUM_ORBITS; ++col)
  {
    for (uint32_t core_num = 0; core_num < num_cores_per_orbit; ++core_num){
      for (uint32_t row = 0; row < NUM_SATS_PER_CORE; ++row)
      {
        nodes.Add(CreateObject<Node>(core_num + (col*num_cores_per_orbit)));
      }
    }
  }

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("80Gbps"));
  p2p.SetChannelAttribute("Delay", StringValue("0.3333ms"));
  p2p.SetQueue("ns3::DropTailQueue",
             "MaxSize", StringValue("6MB"));

  std::vector<GridNode> gridLogic(NUM_ORBITS*NUM_SATS_IN_ORBIT);

auto NodeIndex = [](uint32_t row, uint32_t col) {
return col * NUM_SATS_IN_ORBIT + row;  // column-major for MPI-per-column
};

    for (uint32_t row = 0; row < NUM_SATS_IN_ORBIT; ++row)
    {
    for (uint32_t col = 0; col < NUM_ORBITS; ++col)
    {
        uint32_t id = NodeIndex(row, col);

        // RIGHT (wrap around)
        uint32_t rightCol = (col + 1) % NUM_ORBITS;
        uint32_t rightId  = NodeIndex(row, rightCol);

        // To avoid double-installing links,
        // only create link if col < rightCol OR wrap case
        if (col < rightCol || (col == NUM_ORBITS-1 && rightCol == 0))
        {
        NodeContainer pair(nodes.Get(id), nodes.Get(rightId));
        NetDeviceContainer devs = p2p.Install(pair);

        gridLogic[id].SetDevice("right", devs.Get(0));
        gridLogic[rightId].SetDevice("left", devs.Get(1));
        }

        // DOWN (wrap around)
        uint32_t downRow = (row + 1) % NUM_SATS_IN_ORBIT;
        uint32_t downId  = NodeIndex(downRow, col);

        if (row < downRow || (row == NUM_SATS_IN_ORBIT-1 && downRow == 0))
        {
        NodeContainer pair(nodes.Get(id), nodes.Get(downId));
        NetDeviceContainer devs = p2p.Install(pair);

        gridLogic[id].SetDevice("down", devs.Get(0));
        gridLogic[downId].SetDevice("up", devs.Get(1));
        }
    }
    }

  // Inject traffic only from rank 0
  if (systemId == 0)
  {
    Simulator::Schedule(Seconds(0.1),
                    &MonitorQueues,
                    nodes);
  }

  uint32_t start = systemId * num_cores_per_orbit;
  uint32_t end   = start + num_cores_per_orbit;

  for (uint32_t i = start; i < end; ++i)
  {
    Simulator::Schedule(Seconds(0.0),
                        &GridNode::StartGenerating,
                        &gridLogic[i]);
  }

  Simulator::Stop(Seconds(1000.0));
  Simulator::Run();
  Simulator::Destroy();

  MpiInterface::Disable();
  return 0;
}