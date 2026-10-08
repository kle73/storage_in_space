/**
 * storage_in_space.cc
 * Initiates the simulation
 * Reads TLE files and creates corresponding Satellite and Mobility objects
 * These are assigned to nodes which are assigned to MPI ranks (if enabled)
 *
 * Topology (numOrbits × satellitesPerOrbit fully connected torus)
 * is created and ISL devices are instantiated and connected.
 *
 * SatelliteForwardingApps (satellite-forwarding-app.h/.cc) are created 
 * and assigned to each node.
 *
 * IMPORTANT: This module assumes the TLE files to be sorted and grouped 
              by orbits. 
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-laser-helper.h"
#include "ns3/satellite.h"
#include "ns3/satellite-position-mobility-model.h"
#include "ns3/satellite-forwarding-app.h"
#include "ns3/ring-switch-scheduler.h"

#ifdef NS3_MPI
#include "ns3/mpi-interface.h"
#endif

#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <string>
#include <stdexcept>

#include <stdlib.h>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("StorageInSpace");

// ─────────────────────────────────────────────────────────────────────────────
// TLE file reader
// TLE files must be grouped by orbit and within each orbit the entries must be
// sorted by their mean anomaly.
// ─────────────────────────────────────────────────────────────────────────────
struct TleEntry { 
    std::string name; 
    std::string line1; 
    std::string line2;
 };

struct TleData {
    int64_t numOrbits;
    int64_t satellitesPerOrbit;
    std::vector<TleEntry> entries;
};

static TleData ReadTleFile (const std::string& path)
{
    std::ifstream fs (path);
    if (!fs.is_open ())
        throw std::runtime_error ("Cannot open TLE file: " + path);

    TleData data;
    std::string header;
    std::getline (fs, header);
    {
        std::istringstream iss (header);
        iss >> data.numOrbits >> data.satellitesPerOrbit;
    }

    std::string name, l1, l2;
    while (std::getline (fs, name)) {
        if (name.empty ()) continue;
        if (!std::getline (fs, l1) || !std::getline (fs, l2))
            throw std::runtime_error ("Truncated TLE file");
        data.entries.push_back ({name, l1, l2});
    }

    if ((int64_t)data.entries.size () != data.numOrbits * data.satellitesPerOrbit)
        throw std::runtime_error ("TLE entry count does not match header");

    return data;
}

// ─────────────────────────────────────────────────────────────────────────────
// Grid indexing:  nodeId = orbit * satellitesPerOrbit + position
// ─────────────────────────────────────────────────────────────────────────────
static inline uint32_t NId (int64_t orbit, int64_t pos, int64_t S)
{
    return static_cast<uint32_t> (orbit * S + pos);
}

// ─────────────────────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────────────────────
int main (int argc, char* argv[])
{
    // ── 1. Parameters ────────────────────────────────────────────────────────
    // Set by the run scripts e.g. ns-3.45/run_iridium.sh
    // Relative paths are relative to the folder the simulation runs in (ns-3.45/).
    std::string tleDir              = "scratch/satellite-simulation-data";
    std::string constellation       = "iridium";
    double      simDurationS        = 40.0;
    double      islDataRateMbps     = 80000.0;
    uint32_t    islMaxQueuePkts     = 4000;
    bool        forceStatic         = false;
    bool        nullmsg             = true;  // type of MPI algorithm
    uint8_t     numRanksPerOrbit    = 2;
    bool        useBackpressure     = false;
    float       inclination         = 86.4;
    uint32_t    maxQueueFillLevel   = 1;
    uint32_t    trafficShare        = 2;
    uint32_t    trafficShareBroadcast = 1;
    uint32_t    runNumber           = 0;

    // Routing strategy (SatelliteForwardingApp::SetupWithDevices):
    //   ring-switch-walker-star   ring routing with ring switches, Walker-Star (Iridium)
    //   ring-switch-walker-delta  ring routing with ring switches, Walker-Delta (Starlink)
    std::string routingAlgorithm   = "ring-switch-walker-star";

    // Content generation strategy:
    //   fill-double               fills the system, two copies (UP / DOWN) per object
    std::string        contentGeneration  = "fill-double";

    std::string        statistics         = "standard";

    // Folder for all output files (statistics, debug, positions, ...).
    // Relative paths are resolved against the folder the simulation runs in
    // (e.g. ns-3.45/ when started from there), absolute paths are used as is.
    // The layout below it is fixed (queue_stats/experiment4/..., see
    // SatelliteForwardingApp::StartApplication). Missing folders are created.
    std::string        outDir             = "mysim_results";

    CommandLine cmd;
    cmd.AddValue ("tleDir",             "Folder containing tle-<constellation>.txt", tleDir);
    cmd.AddValue ("constellation",      "LEO constellation to simulate",        constellation);
    cmd.AddValue ("simDur",             "Simulation duration [s]",              simDurationS);
    cmd.AddValue ("islRate",            "ISL data rate [Mbps]",                 islDataRateMbps);
    cmd.AddValue ("islQueue",           "ISL max queue size [packets]",         islMaxQueuePkts);
    cmd.AddValue ("forceStatic",        "Use static satellite positions",       forceStatic);
    cmd.AddValue ("nullmsg",            "Use null-message sync",                nullmsg);
    cmd.AddValue ("numRanksPerOrbit",   "How many MPI ranks for one orbit",     numRanksPerOrbit);
    cmd.AddValue ("useBackpressure",    "",                                     useBackpressure);
    cmd.AddValue ("inclination",        "",                                     inclination);
    cmd.AddValue ("maxQueueFillLevel",  "",                                     maxQueueFillLevel);
    cmd.AddValue ("routingAlgorithm",   "ring-switch-walker-star | ring-switch-walker-delta", routingAlgorithm);
    cmd.AddValue ("contentGeneration",  "fill-double",                          contentGeneration);
    cmd.AddValue ("statistics",         "",                                     statistics);
    cmd.AddValue ("trafficShare",       "TDMA slots per period for the data (storage) queue of each laser device", trafficShare);
    cmd.AddValue ("trafficShareBroadcast", "TDMA slots per period for the broadcast queue of each laser device "
                                        "(0: the data queue gets the whole link)", trafficShareBroadcast);
    cmd.AddValue ("runNumber",          "",                                     runNumber);
    cmd.AddValue ("outDir",             "Folder for all output files (relative to the run folder, or absolute)", outDir);
    cmd.Parse (argc, argv);

    // Fail early on unknown strategy names (keep this list in sync with
    // SatelliteForwardingApp::SetupWithDevices; its check is compiled out in
    // optimized builds).
    if (routingAlgorithm != "ring-switch-walker-star" && routingAlgorithm != "ring-switch-walker-delta")
        NS_FATAL_ERROR ("Unknown --routingAlgorithm=" << routingAlgorithm
                        << " (ring-switch-walker-star | ring-switch-walker-delta)");
    if (contentGeneration != "fill-double")
        NS_FATAL_ERROR ("Unknown --contentGeneration=" << contentGeneration << " (fill-double)");

    // All components write below this folder (SatelliteForwardingApp::OutputPath).
    SatelliteForwardingApp::SetOutputDir (outDir);

    // Output file to store ISL connections created by this module
    const std::string isl_connections =
        SatelliteForwardingApp::OutputPath ("positions/isl_connections.csv");
    FILE* f = fopen(isl_connections.c_str (), "w");
    if (f) { fclose(f); }


    // ── 2. MPI initialisation ────────────────────────────────────────────────
#ifdef NS3_MPI
    if (nullmsg)
    {
      GlobalValue::Bind("SimulatorImplementationType",
        StringValue("ns3::NullMessageSimulatorImpl"));
    } else 
    {
      GlobalValue::Bind("SimulatorImplementationType",
        StringValue("ns3::DistributedSimulatorImpl"));
    }
    MpiInterface::Enable (&argc, &argv);
    uint32_t systemId    = MpiInterface::GetSystemId ();
    uint32_t systemCount = MpiInterface::GetSize ();
#else
    uint32_t systemId    = 0;
    uint32_t systemCount = 1;
#endif

    // ── 3. Read TLE file ─────────────────────────────────────────────────────
    TleData tleData = ReadTleFile (tleDir + "/tle-" + constellation + ".txt");
    int64_t O       = tleData.numOrbits;
    const int64_t S = tleData.satellitesPerOrbit;
    const int64_t N = O * S;

    // Print some information
    if (systemId == 0)
    {
        std::cout << "[StorageInSpace]"
                  << "  orbits=" << O
                  << "  sats/orbit=" << S
                  << "  total=" << N
                  << "  MPI ranks=" << systemCount << std::endl;
        // laser devices: fixed TDMA slot grid, unused slots of a class stay idle
        std::cout << "[StorageInSpace]  ISL slots per period: data " << trafficShare
                  << ", broadcast " << trafficShareBroadcast << "  -> data queue gets "
                  << (trafficShare + trafficShareBroadcast > 0
                          ? islDataRateMbps * trafficShare / (trafficShare + trafficShareBroadcast) / 1000.0
                          : 0.0)
                  << " of " << islDataRateMbps / 1000.0 << " Gbps" << std::endl;
        std::error_code ec;
        const std::filesystem::path outAbs =
            std::filesystem::absolute (SatelliteForwardingApp::GetOutputDir (), ec);
        std::cout << "[StorageInSpace]  output folder: "
                  << (ec ? SatelliteForwardingApp::GetOutputDir () : outAbs.lexically_normal ().string ())
                  << std::endl;
    }

    if (systemCount != 1 && (int64_t)systemCount != O * numRanksPerOrbit)
        NS_FATAL_ERROR ("Run with 1 rank (debug) or exactly " << O * numRanksPerOrbit 
                        << " ranks (one per orbit).");


    // ── 4. Create nodes and assign MPI system-ids ────────────────────────────
    NodeContainer nodes;
    nodes.Create (static_cast<uint32_t> (N));

#ifdef NS3_MPI
    // Rank assignment (only with several MPI processes; a single process keeps
    // all satellites on rank 0, the default SystemId):
    // assigns satellites to MPI ranks such that the rest (r) is split across the first 
    // r ranks (one each) if numSatsPerOrbit (S) % numRanksPerOrbit = r > 0
    if (systemCount > 1)
    {
    uint16_t num_per_rank_low = S / numRanksPerOrbit; 

	for (int orbit=0; orbit<O; orbit++) 
	{
		uint16_t rank_in_orbit = 0;
		uint16_t rest = S % numRanksPerOrbit;             
		uint8_t done = 0;
		for (int sat=0; sat<S; sat++)
		{
			uint16_t sat_num = orbit*S + sat;
			uint32_t rank = rank_in_orbit + orbit*numRanksPerOrbit;
            nodes.Get (static_cast<uint32_t>(sat_num))
                ->SetAttribute ("SystemId", UintegerValue (rank));
			if (rest > 0)
			{
				if ((sat+1) % (num_per_rank_low+1) == 0)
				{
					rest--;
					rank_in_orbit++;
					if (rest == 0){
						done = sat+1;
					}
				}
			}else if (rest == 0) 
			{
				if((sat-done+1) % num_per_rank_low == 0)
				{
					rank_in_orbit++;
				}
			}
		}
	}
    }
#endif

    // ── 5. Mobility models  ────────────────
    for (int64_t i = 0; i < N; ++i)
    {
        Ptr<Node> node = nodes.Get (static_cast<uint32_t>(i));
        // NO MPI skip here — all ranks need mobility on all nodes
        // because p2p.Install() calls GetDistanceFrom() on both endpoints

        const TleEntry& tle = tleData.entries[static_cast<size_t>(i)];
        Ptr<Satellite> sat = CreateObject<Satellite> ();
        sat->SetName    (tle.name);
        sat->SetTleInfo (tle.line1, tle.line2);

        MobilityHelper mob;
        if (forceStatic) {
            mob.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
            mob.Install (node);
            node->GetObject<MobilityModel> ()
                ->SetPosition (sat->GetPosition (sat->GetTleEpoch ()));
        } else {
            mob.SetMobilityModel (
                "ns3::SatellitePositionMobilityModel",
                "SatellitePositionHelper",
                SatellitePositionHelperValue (SatellitePositionHelper (sat)));
            mob.Install (node);
        }
    }

    // ── 6. Install ISL links and build per-node device tables ────────────────
    //
    // We install links in two rounds so we can record which device goes in
    // which direction, without depending on device-index ordering.
    //
    // devUp[i]    = net-device on node i that leads to node i's UP neighbour
    // devDown[i]  = net-device on node i that leads to node i's DOWN neighbour
    // devRight[i] = net-device on node i that leads to node i's RIGHT neighbour
    // devLeft[i]  = net-device on node i that leads to node i's LEFT neighbour

    std::vector<Ptr<NetDevice>> devUp   (static_cast<size_t>(N));
    std::vector<Ptr<NetDevice>> devDown (static_cast<size_t>(N));
    std::vector<Ptr<NetDevice>> devRight(static_cast<size_t>(N));
    std::vector<Ptr<NetDevice>> devLeft (static_cast<size_t>(N));

    // Info for the Scheduler Module
    std::vector<uint32_t>              islRightPartner(static_cast<size_t>(N), UINT32_MAX);
    std::vector<std::vector<uint32_t>> orbits(O, std::vector<uint32_t>(S, UINT32_MAX));

    PointToPointLaserHelper p2p;
    std::string qStr = std::to_string (islMaxQueuePkts) + "p";
    p2p.SetQueue ("ns3::DropTailQueue<Packet>",
                  "MaxSize", QueueSizeValue (QueueSize (qStr)));
    p2p.SetDeviceAttribute ("DataRate",
                            DataRateValue (DataRate (
                                std::to_string (islDataRateMbps) + "Mbps")));
    const uint32_t laserMtu = SatelliteForwardingApp::maxPayloadSize
                        + RingSwitchStarHeader ().GetSerializedSize ();   // 1500 + 42
    p2p.SetDeviceAttribute ("Mtu", UintegerValue (laserMtu + 6)); // we need 1548
    p2p.SetDeviceAttribute ("TrafficShare", UintegerValue (trafficShare));
    NS_ABORT_MSG_IF (trafficShare == 0 && trafficShareBroadcast == 0,
                     "--trafficShare and --trafficShareBroadcast cannot both be 0");
    p2p.SetDeviceAttribute ("TrafficShareBroadcast", UintegerValue (trafficShareBroadcast));

    

    // ── Helper: Euclidean distance between two nodes ──────────────────────────────
    auto getPos = [&](uint32_t id) {
        return nodes.Get(id)->GetObject<MobilityModel>()->GetPosition();
    };
    auto euclidean = [&](uint32_t idA, uint32_t idB) -> double {
        Vector a = getPos(idA), b = getPos(idB);
        double dx = a.x-b.x, dy = a.y-b.y, dz = a.z-b.z;
        return std::sqrt(dx*dx + dy*dy + dz*dz);
    };

    // ── Round A: UP/DOWN links — closest neighbour in same orbit ──────────────────
    // Within a circular orbit, each satellite has exactly two neighbours.
    // We connect (o,p) → (o,(p+1)%S) — the ring order in the TLE files
    // already matches physical orbital order (sorted by Mean Anomaly), so
    // adjacent slots are always the two physically closest satellites.
    for (int64_t o = 0; o < O; ++o)
    {
        for (int64_t p = 0; p < S; ++p)
        {
            int64_t  pNext = (p + 1) % S;
            uint32_t idA   = NId (o, p,     S);
            uint32_t idB   = NId (o, pNext, S);

            NodeContainer nc;
            nc.Add (nodes.Get (idA));
            nc.Add (nodes.Get (idB));
            NetDeviceContainer ndc = p2p.Install (nc);

            devUp  [idA] = ndc.Get (0);
            devDown[idB] = ndc.Get (1);

            orbits[o][p] = idA;

            if (systemId == 0){
                f = fopen(isl_connections.c_str (), "a");
                if (f) { fprintf(f, "Node %u --- UP --- %u\n", idA, idB); fclose(f); }
            }
        }
    }

    // ── Round B: RIGHT/LEFT links — closest satellite in adjacent orbit ───────────
    //
    // For each satellite (o,p) we find the slot q in orbit (o+1)%O whose
    // current position is closest. We then install the link (o,p)↔(o+1,q).
    //
    // To guarantee a valid matching (no satellite gets two RIGHT-neighbours or
    // two LEFT-neighbours) we use a greedy minimum-weight matching:
    //   - Sort all O*S candidate pairs by distance.
    //   - Accept a pair only if neither endpoint has been matched yet.
    //
    // This produces a perfect 1-to-1 matching per orbit-pair.

    for (int64_t o = 0; o < O; ++o)
    {
        int64_t oNext = (o + 1) % O;

        // Build all S×S candidate pairs with distances
        struct Candidate { double dist; int64_t p, q; };
        std::vector<Candidate> candidates;
        candidates.reserve(static_cast<size_t>(S * S));

        for (int64_t p = 0; p < S; ++p)
            for (int64_t q = 0; q < S; ++q)
                candidates.push_back({
                    euclidean(NId(o, p, S), NId(oNext, q, S)),
                    p, q
                });

        std::sort(candidates.begin(), candidates.end(),
                [](const Candidate& a, const Candidate& b){
                    return a.dist < b.dist;
                });

        // Greedy matching
        std::vector<bool> usedP(static_cast<size_t>(S), false);
        std::vector<bool> usedQ(static_cast<size_t>(S), false);
        int64_t matched = 0;

        for (const auto& c : candidates)
        {
            if (matched == S) break;
            if (usedP[static_cast<size_t>(c.p)] || usedQ[static_cast<size_t>(c.q)])
                continue;

            usedP[static_cast<size_t>(c.p)] = true;
            usedQ[static_cast<size_t>(c.q)] = true;
            ++matched;

            uint32_t idA = NId(o,     c.p, S);
            uint32_t idB = NId(oNext, c.q, S);

            NodeContainer nc;
            nc.Add (nodes.Get (idA));
            nc.Add (nodes.Get (idB));
            NetDeviceContainer ndc = p2p.Install (nc);

            devRight[idA] = ndc.Get (0);
            devLeft [idB] = ndc.Get (1);

            islRightPartner[idA] = idB; 

            if (systemId == 0){
                f = fopen(isl_connections.c_str (), "a");
                if (f) { fprintf(f, "Node %u --- Right --- %u\n", idA, idB); fclose(f); }
            }
        }

        NS_ASSERT_MSG(matched == S,
            "RIGHT/LEFT matching failed for orbit " << o << ": only "
            << matched << "/" << S << " pairs found.");
    }

    if (systemId == 0){
        for (int o=0; o<O; o++){
            for (int p=0; p<S; p++){
                printf("Orbit %d: %u --> %u\n", o, orbits[o][p], islRightPartner[orbits[o][p]]);
            }
        }
    }

    // ── 7. Install forwarding application on local nodes ─────────────────────

    // Collect app pointers (indexed by nodeId) for the ring-switch scheduler.
    std::vector<SatelliteForwardingApp*> allApps(static_cast<size_t>(N), nullptr);

    for (int64_t i = 0; i < N; ++i)
    {
        Ptr<Node> node = nodes.Get (static_cast<uint32_t>(i));

#ifdef NS3_MPI
        if (systemCount > 1 && node->GetSystemId () != systemId) continue;
#endif
        size_t si = static_cast<size_t>(i);
        bool isSeed = (i == 0 && systemId == 0);

        Ptr<SatelliteForwardingApp> app = CreateObject<SatelliteForwardingApp> ();
        NS_ASSERT_MSG (devRight[si],
        "STORAGE IN SPACE : null device on sat " << si);
        app->SetupWithDevices (
            static_cast<uint32_t> (S),
            static_cast<uint32_t> (N),
            devUp[si], devDown[si], devRight[si], devLeft[si],
            isSeed,
            inclination,
            maxQueueFillLevel,
            routingAlgorithm,
            contentGeneration,
            statistics,
            useBackpressure,
            MakeConstellationConfig (constellation),
            trafficShare,
            runNumber);

        node->AddApplication (app);
        app->SetStartTime (Seconds (0.0));
        app->SetStopTime  (Seconds (simDurationS));
        allApps[si] = PeekPointer(app);
    }

    // ── 8 Dynamic ring-switch scheduler ─────────────────
    // The scheduler pre-computes geometry-optimal switch times and drives continuous
    // ring switching. Only needed for ring-double
    RingSwitchScheduler ringScheduler;
    if (routingAlgorithm == "ring-switch-walker-star" || routingAlgorithm == "ring-switch-walker-delta") {
        ConstellationConfig schedCfg = MakeConstellationConfig(constellation);
        ringScheduler.Setup(schedCfg, static_cast<uint32_t>(S), static_cast<uint32_t>(O),
                             allApps, nodes, islRightPartner, orbits, systemId);
        // Wire scheduler pointer into each local app so terminus can call back.
        for (auto* ap : allApps) {
            if (ap) ap->m_ringScheduler = &ringScheduler;
        }
        ringScheduler.Start();
    }

    // ── 8. Run ────────────────────────────────────────────────────────────────
    Simulator::Stop (Seconds (simDurationS));
    Simulator::Run ();
    Simulator::Destroy ();

#ifdef NS3_MPI
    MpiInterface::Disable ();
#endif

    if (systemId == 0)
        std::cout << "[StorageInSpace] Done." << std::endl;

    return 0;
}