// Storage in Space: main program.
//
// Reads the TLE file of the constellation, creates one node per satellite with
// its orbit (SGP4) mobility model, connects the satellites with laser ISLs,
// installs a SatelliteForwardingApp on every satellite of this MPI rank, sets
// up the ring-switch scheduler and runs the simulation.
//
// Node id = orbit * satellitesPerOrbit + position in the orbit. The TLE file
// must therefore be grouped by orbit and sorted by mean anomaly within each
// orbit.

#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-laser-helper.h"
#include "ns3/ring-switch-scheduler.h"
#include "ns3/routing-ring-switch-walker-delta.h"
#include "ns3/satellite-forwarding-app.h"
#include "ns3/satellite-position-mobility-model.h"
#include "ns3/satellite.h"

#ifdef NS3_MPI
#include "ns3/mpi-interface.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StorageInSpace");

namespace
{

struct TleEntry
{
    std::string name;
    std::string line1;
    std::string line2;
};

struct TleData
{
    uint32_t numOrbits{0};
    uint32_t satellitesPerOrbit{0};
    std::vector<TleEntry> entries;
};

/// Reads a TLE file: first line "<orbits> <satellites per orbit>", then three
/// lines (name, line 1, line 2) per satellite.
TleData
ReadTleFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        throw std::runtime_error("Cannot open TLE file: " + path);
    }

    TleData data;
    std::string line;
    std::getline(file, line);
    std::istringstream(line) >> data.numOrbits >> data.satellitesPerOrbit;

    std::string name;
    std::string line1;
    std::string line2;
    while (std::getline(file, name))
    {
        if (name.empty())
        {
            continue;
        }
        if (!std::getline(file, line1) || !std::getline(file, line2))
        {
            throw std::runtime_error("Truncated TLE file: " + path);
        }
        data.entries.push_back({name, line1, line2});
    }
    if (data.entries.size() != static_cast<std::size_t>(data.numOrbits) * data.satellitesPerOrbit)
    {
        throw std::runtime_error("TLE entry count does not match the header: " + path);
    }
    return data;
}

/// MPI rank (within its orbit) of the satellite at `position`: every orbit is
/// split into `ranksPerOrbit` consecutive blocks; the first blocks get one
/// satellite more if the orbit cannot be split evenly.
uint32_t
RankInOrbit(uint32_t position, uint32_t satellitesPerOrbit, uint32_t ranksPerOrbit)
{
    const uint32_t small = satellitesPerOrbit / ranksPerOrbit;
    const uint32_t numLarge = satellitesPerOrbit % ranksPerOrbit;
    const uint32_t large = small + 1;
    if (position < numLarge * large)
    {
        return position / large;
    }
    return numLarge + (position - numLarge * large) / small;
}

} // namespace

int
main(int argc, char* argv[])
{
    // ── Parameters ───────────────────────────────────────────────────────────
    // Relative paths are relative to the folder the simulation runs in.
    std::string tleDir = "scratch/satellite-simulation-data";
    std::string constellation = "iridium";
    std::string routingAlgorithm = "ring-switch-walker-star";
    std::string contentGeneration = "fill-double";
    const std::vector<std::string> broadcastAlgorithms = RoutingRingSwitchDelta::GetBroadcastAlgorithmNames();
    std::string broadcastAlgorithm = broadcastAlgorithms.front();
    std::string outDir = "mysim_results";
    double simDurationS = 40.0;
    double islDataRateMbps = 80000.0;
    uint32_t islQueueSize = 4000;
    uint32_t maxQueueFillLevel = 1;
    uint32_t objectSize = 10;
    uint64_t objectTtl = 10000;
    uint32_t trafficShare = 2;
    uint32_t trafficShareBroadcast = 1;
    uint32_t numRanksPerOrbit = 2;
    uint32_t runNumber = 0;
    bool nullMessage = true;
    bool forceStatic = false;
    // accepted for compatibility with existing run scripts, not used
    bool useBackpressure = false;
    double inclination = 86.4;
    std::string statistics = "standard";

    CommandLine cmd;
    cmd.AddValue("tleDir", "Folder containing tle-<constellation>.txt", tleDir);
    cmd.AddValue("constellation", "Constellation (iridium | starlink)", constellation);
    cmd.AddValue("routingAlgorithm", "ring-switch-walker-star | ring-switch-walker-delta", routingAlgorithm);
    cmd.AddValue("contentGeneration", "fill-double | broadcast", contentGeneration);
    std::string broadcastAlgorithmList;
    for (const std::string& name : broadcastAlgorithms)
    {
        broadcastAlgorithmList += (broadcastAlgorithmList.empty() ? "" : " | ") + name;
    }
    cmd.AddValue("broadcastAlgorithm",
                 "Broadcast algorithm of ring-switch-walker-delta: " + broadcastAlgorithmList,
                 broadcastAlgorithm);
    cmd.AddValue("outDir", "Folder for all output files (relative to the run folder, or absolute)", outDir);
    cmd.AddValue("simDur", "Simulation duration [s]", simDurationS);
    cmd.AddValue("islRate", "ISL data rate [Mbps]", islDataRateMbps);
    cmd.AddValue("islQueue", "ISL queue size [packets]", islQueueSize);
    cmd.AddValue("maxQueueFillLevel", "Fill level of the storage queues [% of islQueue]", maxQueueFillLevel);
    cmd.AddValue("objectSize", "Packets per object", objectSize);
    cmd.AddValue("ttl", "Time to live of stored objects [s]", objectTtl);
    cmd.AddValue("trafficShare",
                 "TDMA slots per period for the data (storage) queue of each laser device",
                 trafficShare);
    cmd.AddValue("trafficShareBroadcast",
                 "TDMA slots per period for the broadcast queue of each laser device "
                 "(0: the data queue gets the whole link)",
                 trafficShareBroadcast);
    cmd.AddValue("numRanksPerOrbit", "MPI ranks per orbit", numRanksPerOrbit);
    cmd.AddValue("runNumber", "Appended to the names of the output files", runNumber);
    cmd.AddValue("nullmsg", "MPI synchronisation: null messages (true) or distributed (false)", nullMessage);
    cmd.AddValue("forceStatic", "Keep the satellites at their initial positions", forceStatic);
    cmd.AddValue("useBackpressure", "Not used", useBackpressure);
    cmd.AddValue("inclination", "Not used", inclination);
    cmd.AddValue("statistics", "Not used", statistics);
    cmd.Parse(argc, argv);

    if (routingAlgorithm != "ring-switch-walker-star" && routingAlgorithm != "ring-switch-walker-delta")
    {
        NS_FATAL_ERROR("Unknown --routingAlgorithm=" << routingAlgorithm
                                                     << " (ring-switch-walker-star | ring-switch-walker-delta)");
    }
    if (contentGeneration != "fill-double" && contentGeneration != "broadcast")
    {
        NS_FATAL_ERROR("Unknown --contentGeneration=" << contentGeneration << " (fill-double | broadcast)");
    }
    if (std::find(broadcastAlgorithms.begin(), broadcastAlgorithms.end(), broadcastAlgorithm) ==
        broadcastAlgorithms.end())
    {
        NS_FATAL_ERROR("Unknown --broadcastAlgorithm=" << broadcastAlgorithm << " (" << broadcastAlgorithmList
                                                       << ")");
    }
    NS_ABORT_MSG_IF(trafficShare == 0 && trafficShareBroadcast == 0,
                    "--trafficShare and --trafficShareBroadcast cannot both be 0");
    NS_ABORT_MSG_IF(numRanksPerOrbit == 0, "--numRanksPerOrbit must be at least 1");

    SatelliteForwardingApp::SetOutputDir(outDir);

    // ── MPI ──────────────────────────────────────────────────────────────────
#ifdef NS3_MPI
    GlobalValue::Bind("SimulatorImplementationType",
                      StringValue(nullMessage ? "ns3::NullMessageSimulatorImpl"
                                              : "ns3::DistributedSimulatorImpl"));
    MpiInterface::Enable(&argc, &argv);
    const uint32_t systemId = MpiInterface::GetSystemId();
    const uint32_t systemCount = MpiInterface::GetSize();
#else
    const uint32_t systemId = 0;
    const uint32_t systemCount = 1;
#endif

    // ── Constellation ────────────────────────────────────────────────────────
    const TleData tle = ReadTleFile(tleDir + "/tle-" + constellation + ".txt");
    const uint32_t numOrbits = tle.numOrbits;
    const uint32_t satsPerOrbit = tle.satellitesPerOrbit;
    const uint32_t numSats = numOrbits * satsPerOrbit;
    auto satId = [satsPerOrbit](uint32_t orbit, uint32_t position) { return orbit * satsPerOrbit + position; };

    if (systemId == 0)
    {
        std::cout << "[StorageInSpace]  orbits=" << numOrbits << "  sats/orbit=" << satsPerOrbit
                  << "  total=" << numSats << "  MPI ranks=" << systemCount << std::endl;
        std::error_code ec;
        const std::filesystem::path outAbs =
            std::filesystem::absolute(SatelliteForwardingApp::GetOutputDir(), ec);
        std::cout << "[StorageInSpace]  output folder: "
                  << (ec ? SatelliteForwardingApp::GetOutputDir() : outAbs.lexically_normal().string())
                  << std::endl;
        std::cout << "[StorageInSpace]  ISL slots per period: data " << trafficShare << ", broadcast "
                  << trafficShareBroadcast << "  -> data queue gets "
                  << islDataRateMbps * trafficShare / (trafficShare + trafficShareBroadcast) / 1000.0 << " of "
                  << islDataRateMbps / 1000.0 << " Gbps" << std::endl;
        if (contentGeneration == "broadcast" && routingAlgorithm == "ring-switch-walker-delta")
        {
            std::cout << "[StorageInSpace]  broadcast algorithm: " << broadcastAlgorithm << std::endl;
        }
    }
    if (systemCount != 1 && systemCount != numOrbits * numRanksPerOrbit)
    {
        NS_FATAL_ERROR("Run with 1 process or with exactly " << numOrbits * numRanksPerOrbit
                                                              << " processes (orbits x numRanksPerOrbit)");
    }

    NodeContainer nodes;
    nodes.Create(numSats);
    if (systemCount > 1)
    {
        for (uint32_t orbit = 0; orbit < numOrbits; orbit++)
        {
            for (uint32_t position = 0; position < satsPerOrbit; position++)
            {
                const uint32_t rank =
                    orbit * numRanksPerOrbit + RankInOrbit(position, satsPerOrbit, numRanksPerOrbit);
                nodes.Get(satId(orbit, position))->SetAttribute("SystemId", UintegerValue(rank));
            }
        }
    }

    // Every rank needs the mobility of all nodes (the ISL delays depend on it).
    for (uint32_t i = 0; i < numSats; i++)
    {
        Ptr<Satellite> satellite = CreateObject<Satellite>();
        satellite->SetName(tle.entries[i].name);
        satellite->SetTleInfo(tle.entries[i].line1, tle.entries[i].line2);

        MobilityHelper mobility;
        if (forceStatic)
        {
            mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
            mobility.Install(nodes.Get(i));
            nodes.Get(i)->GetObject<MobilityModel>()->SetPosition(
                satellite->GetPosition(satellite->GetTleEpoch()));
        }
        else
        {
            mobility.SetMobilityModel("ns3::SatellitePositionMobilityModel",
                                      "SatellitePositionHelper",
                                      SatellitePositionHelperValue(SatellitePositionHelper(satellite)));
            mobility.Install(nodes.Get(i));
        }
    }

    // ── ISLs ─────────────────────────────────────────────────────────────────
    PointToPointLaserHelper laser;
    laser.SetQueue("ns3::DropTailQueue<Packet>",
                   "MaxSize",
                   QueueSizeValue(QueueSize(std::to_string(islQueueSize) + "p")));
    laser.SetDeviceAttribute("DataRate", DataRateValue(DataRate(std::to_string(islDataRateMbps) + "Mbps")));
    // Payload + header = 1542 bytes. With an MTU of 1548 a TDMA slot (MTU + 2
    // bytes PPP header) lasts a whole number of nanoseconds at the usual rates.
    const uint32_t mtu = SatelliteForwardingApp::kPayloadSize + SatPacketHeader().GetSerializedSize() + 6;
    laser.SetDeviceAttribute("Mtu", UintegerValue(mtu));
    laser.SetDeviceAttribute("TrafficShare", UintegerValue(trafficShare));
    laser.SetDeviceAttribute("TrafficShareBroadcast", UintegerValue(trafficShareBroadcast));

    std::vector<Ptr<NetDevice>> devUp(numSats);
    std::vector<Ptr<NetDevice>> devDown(numSats);
    std::vector<Ptr<NetDevice>> devRight(numSats);
    std::vector<Ptr<NetDevice>> devLeft(numSats);
    // for the ring-switch scheduler
    std::vector<uint32_t> rightPartner(numSats, UINT32_MAX);

    const std::string islFile = SatelliteForwardingApp::OutputPath("positions/isl_connections.csv");
    FILE* islLog = (systemId == 0) ? std::fopen(islFile.c_str(), "w") : nullptr;

    // Up / down: consecutive satellites of an orbit (the TLE order is the
    // order along the orbit).
    for (uint32_t orbit = 0; orbit < numOrbits; orbit++)
    {
        for (uint32_t position = 0; position < satsPerOrbit; position++)
        {
            const uint32_t a = satId(orbit, position);
            const uint32_t b = satId(orbit, (position + 1) % satsPerOrbit);
            NetDeviceContainer devices = laser.Install(nodes.Get(a), nodes.Get(b));
            devUp[a] = devices.Get(0);
            devDown[b] = devices.Get(1);
            if (islLog)
            {
                std::fprintf(islLog, "Node %u --- UP --- %u\n", a, b);
            }
        }
    }

    // Right / left: every satellite is connected to one satellite of the next
    // orbit. Greedy minimum-distance matching on the initial positions: all
    // pairs sorted by distance, a pair is taken if both satellites are free.
    auto distance = [&nodes](uint32_t a, uint32_t b) {
        const Vector pa = nodes.Get(a)->GetObject<MobilityModel>()->GetPosition();
        const Vector pb = nodes.Get(b)->GetObject<MobilityModel>()->GetPosition();
        return CalculateDistance(pa, pb);
    };
    for (uint32_t orbit = 0; orbit < numOrbits; orbit++)
    {
        const uint32_t next = (orbit + 1) % numOrbits;
        struct Candidate
        {
            double distance;
            uint32_t p;
            uint32_t q;
        };

        std::vector<Candidate> candidates;
        candidates.reserve(static_cast<std::size_t>(satsPerOrbit) * satsPerOrbit);
        for (uint32_t p = 0; p < satsPerOrbit; p++)
        {
            for (uint32_t q = 0; q < satsPerOrbit; q++)
            {
                candidates.push_back({distance(satId(orbit, p), satId(next, q)), p, q});
            }
        }
        std::sort(candidates.begin(), candidates.end(), [](const Candidate& x, const Candidate& y) {
            return x.distance < y.distance;
        });

        std::vector<bool> usedP(satsPerOrbit, false);
        std::vector<bool> usedQ(satsPerOrbit, false);
        uint32_t matched = 0;
        for (const Candidate& c : candidates)
        {
            if (matched == satsPerOrbit)
            {
                break;
            }
            if (usedP[c.p] || usedQ[c.q])
            {
                continue;
            }
            usedP[c.p] = true;
            usedQ[c.q] = true;
            matched++;

            const uint32_t a = satId(orbit, c.p);
            const uint32_t b = satId(next, c.q);
            NetDeviceContainer devices = laser.Install(nodes.Get(a), nodes.Get(b));
            devRight[a] = devices.Get(0);
            devLeft[b] = devices.Get(1);
            rightPartner[a] = b;
            if (islLog)
            {
                std::fprintf(islLog, "Node %u --- Right --- %u\n", a, b);
            }
        }
        NS_ABORT_MSG_IF(matched != satsPerOrbit, "Right/left matching failed for orbit " << orbit);
    }
    if (islLog)
    {
        std::fclose(islLog);
    }

    // ── Applications ─────────────────────────────────────────────────────────
    SatelliteForwardingAppParams params;
    params.satellitesPerOrbit = satsPerOrbit;
    params.numSatellites = numSats;
    params.routingAlgorithm = routingAlgorithm;
    params.contentGeneration = contentGeneration;
    params.broadcastAlgorithm = broadcastAlgorithm;
    params.constellation = MakeConstellationConfig(constellation);
    params.maxQueueFillLevel = maxQueueFillLevel;
    params.objectSize = objectSize;
    params.objectTtl = objectTtl;
    params.runNumber = runNumber;

    // indexed by node id, nullptr for the satellites of other ranks
    std::vector<SatelliteForwardingApp*> apps(numSats, nullptr);
    for (uint32_t i = 0; i < numSats; i++)
    {
        Ptr<Node> node = nodes.Get(i);
        if (systemCount > 1 && node->GetSystemId() != systemId)
        {
            continue;
        }
        params.devUp = devUp[i];
        params.devDown = devDown[i];
        params.devRight = devRight[i];
        params.devLeft = devLeft[i];

        Ptr<SatelliteForwardingApp> app = CreateObject<SatelliteForwardingApp>();
        app->Setup(params);
        node->AddApplication(app);
        app->SetStartTime(Seconds(0.0));
        app->SetStopTime(Seconds(simDurationS));
        apps[i] = PeekPointer(app);
    }

    // ── Ring switches ────────────────────────────────────────────────────────
    RingSwitchScheduler ringScheduler(params.constellation, satsPerOrbit, numOrbits, nodes, rightPartner, apps);
    ringScheduler.ScheduleSwitches(simDurationS);

    // ── Run ──────────────────────────────────────────────────────────────────
    Simulator::Stop(Seconds(simDurationS));
    Simulator::Run();
    Simulator::Destroy();

#ifdef NS3_MPI
    MpiInterface::Disable();
#endif

    if (systemId == 0)
    {
        std::cout << "[StorageInSpace] Done." << std::endl;
    }
    return 0;
}
