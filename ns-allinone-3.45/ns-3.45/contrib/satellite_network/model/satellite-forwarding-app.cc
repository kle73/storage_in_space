/*
* satellite-forwarding-app.cc
*
* Main apllication of each satellite. 
* Registers content- and routing strategy
* Implements monitoring to gather statistics of e.g. queue fill level,
* dropped packets, ...
*
*/
#include "satellite-forwarding-app.h"
#include "ring-switch-scheduler.h"


#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/mac48-address.h"
#include "ns3/queue.h"
#include "ns3/point-to-point-laser-net-device.h"
#include "ns3/vector.h"
#include "ns3/mobility-model.h"

#ifdef NS3_MPI
#include "ns3/mpi-interface.h"
#endif

#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <system_error>

NS_LOG_COMPONENT_DEFINE ("SatelliteForwardingApp");

namespace ns3 {

// ─────────────────────────────────────────────────────────────────────────────
// Output folder (see SatelliteForwardingApp::SetOutputDir in the header)
// ─────────────────────────────────────────────────────────────────────────────
namespace {
std::string& OutputDirStorage ()
{
    static std::string dir = "mysim_results";   // relative to the working directory
    return dir;
}
} // namespace

void SatelliteForwardingApp::SetOutputDir (const std::string& dir)
{
    std::string d = dir.empty () ? std::string (".") : dir;
    while (d.size () > 1 && d.back () == '/') d.pop_back ();   // "results/" -> "results"
    OutputDirStorage () = d;
}

std::string SatelliteForwardingApp::GetOutputDir ()
{
    return OutputDirStorage ();
}

std::string SatelliteForwardingApp::OutputPath (const std::string& relPath)
{
    namespace fs = std::filesystem;
    const fs::path p = fs::path (OutputDirStorage ()) / relPath;
    if (p.has_parent_path ())
    {
        // Several MPI ranks may get here at the same time: ignore the error
        // code and only check that the folder exists afterwards.
        std::error_code ec;
        fs::create_directories (p.parent_path (), ec);
        if (!fs::is_directory (p.parent_path (), ec))
            std::cerr << "[SatelliteForwardingApp] WARNING: cannot create output folder "
                      << p.parent_path ().string () << std::endl;
    }
    return p.string ();
}

void SatelliteForwardingApp::OutputPath (const std::string& relPath, char* dst, std::size_t dstSize)
{
    const std::string p = OutputPath (relPath);
    if (p.size () >= dstSize)
        NS_FATAL_ERROR ("Output path too long (" << p.size () << " characters, max "
                        << dstSize - 1 << "): " << p << "  -> use a shorter --outDir");
    std::memcpy (dst, p.c_str (), p.size () + 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// SatForwardHeader
// ─────────────────────────────────────────────────────────────────────────────
NS_OBJECT_ENSURE_REGISTERED (SatForwardHeader);

TypeId SatForwardHeader::GetTypeId (void)
{
    static TypeId tid = TypeId ("ns3::SatForwardHeader")
        .SetParent<Header> ()
        .SetGroupName ("SatelliteNetwork")
        .AddConstructor<SatForwardHeader> ();
    return tid;
}

TypeId SatForwardHeader::GetInstanceTypeId (void) const { return GetTypeId (); }

void SatForwardHeader::Serialize (Buffer::Iterator start) const
{
    int32_t n_bits;
    memcpy (&n_bits, &m_n, sizeof (int32_t));
    start.WriteHtonU32 (n_bits);
    start.WriteHtonU32 (m_p);
    start.WriteHtonU32 (m_id);
    start.WriteHtonU32 (m_obj_id);
    start.WriteHtonU32 (m_frag_id);
    start.WriteHtonU16 (m_sat);
    start.WriteHtonU16 (m_cameLeft);
    start.WriteHtonU64 (m_time);
    uint32_t radj_bits;
    memcpy (&radj_bits, &m_radj, sizeof (float));
    start.WriteHtonU32 (radj_bits);
    start.WriteHtonU32 (m_downEpoch);
}

uint32_t SatForwardHeader::Deserialize (Buffer::Iterator start)
{
    int32_t n_bits = start.ReadNtohU32 ();
    memcpy (&m_n, &n_bits, sizeof (int32_t));
    m_p       = start.ReadNtohU32 ();
    m_id      = start.ReadNtohU32 ();
    m_obj_id  = start.ReadNtohU32 ();
    m_frag_id = start.ReadNtohU32 ();
    m_sat     = start.ReadNtohU16 ();
    m_cameLeft= start.ReadNtohU16 ();
    m_time    = start.ReadNtohU64 ();
    uint32_t radj_bits = start.ReadNtohU32 ();
    memcpy (&m_radj, &radj_bits, sizeof (float));
    m_downEpoch = start.ReadNtohU32 ();
    return 40;
}

void SatForwardHeader::Print (std::ostream& os) const
{
    os << "SatForwardHeader n=" << m_n
       << " id=" << m_id
       << " time=" << m_time
       << " sat=" << m_sat;
}

// ─────────────────────────────────────────────────────────────────────────────
// SatelliteForwardingApp — registration & lifecycle
// ─────────────────────────────────────────────────────────────────────────────
NS_OBJECT_ENSURE_REGISTERED (SatelliteForwardingApp);

TypeId SatelliteForwardingApp::GetTypeId (void)
{
    static TypeId tid = TypeId ("ns3::SatelliteForwardingApp")
        .SetParent<Application> ()
        .SetGroupName ("SatelliteNetwork")
        .AddConstructor<SatelliteForwardingApp> ();
    return tid;
}

SatelliteForwardingApp::SatelliteForwardingApp () {}
SatelliteForwardingApp::~SatelliteForwardingApp () {}

// ─────────────────────────────────────────────────────────────────────────────
// Setup — wires devices and instantiates strategies via factory strings
// ─────────────────────────────────────────────────────────────────────────────
void SatelliteForwardingApp::SetupWithDevices (uint32_t       satellitesPerOrbit,
                                               uint32_t       numSatellites,
                                               Ptr<NetDevice> devUp,
                                               Ptr<NetDevice> devDown,
                                               Ptr<NetDevice> devRight,
                                               Ptr<NetDevice> devLeft,
                                               bool           injectSeed,
                                               float          inclination,
                                               uint32_t       maxQueueFillLevel,
                                               std::string    routingAlgorithm,
                                               std::string    contentGeneration,
                                               std::string    statistics,
                                               bool           useBackpressure,
                                               ConstellationConfig topoConfig,
                                               uint32_t       trafficShare,
                                               uint32_t       runNumber)
{
    m_satellitesPerOrbit = satellitesPerOrbit;
    m_numSatellites      = numSatellites;
    m_devUp              = devUp;
    m_devDown            = devDown;
    m_devRight           = devRight;
    m_devLeft            = devLeft;
    m_injectSeed         = injectSeed;
    m_useBackpressure    = useBackpressure;
    m_inclination        = inclination;
    m_maxQueueFillLevel  = maxQueueFillLevel;
    m_topoConfig         = topoConfig;
    m_trafficShare       = trafficShare;
    m_runNumber          = runNumber;

    m_numOrbits = m_numSatellites / m_satellitesPerOrbit;

    // ── Routing strategy factory ──────────────────────────────────────────
    if (routingAlgorithm == "ring-switch-walker-star")                 m_routing = std::make_unique<RoutingRingSwitchStar>();
    else if (routingAlgorithm == "ring-switch-walker-delta")           m_routing = std::make_unique<RoutingRingSwitchDelta>();
    else NS_ASSERT_MSG (false, "Unknown routing algorithm: " << routingAlgorithm);

    // if (m_useBackpressure)
        // m_routing = std::make_unique<RoutingBackpressureWrapper> (std::move (m_routing));

    // ── Content strategy factory ──────────────────────────────────────────
    if (contentGeneration == "fill-double")   m_content = std::make_unique<ContentFillDouble>();
    else NS_ASSERT_MSG (false, "Unknown content generation: " << contentGeneration);

}

// ─────────────────────────────────────────────────────────────────────────────
// StartApplication
// Prepares all Output Files 
// Initiates RNG
// Sets Parameters (queue sizes, ...)
// Schedules all Monitoring and starts content generation
// ─────────────────────────────────────────────────────────────────────────────
void SatelliteForwardingApp::StartApplication (void)
{
    NS_ASSERT_MSG (m_satellitesPerOrbit > 0, "Call SetupWithDevices() before starting");

    // Output files, relative to the output folder (--outDir, default
    // mysim_results/); missing folders are created by OutputPath.
    const std::string run = std::to_string (m_runNumber);
    OutputPath ("queue_stats/experiment4/queue_statisticsS" + run + ".csv",        filename_queue_stats,     sizeof (filename_queue_stats));
    OutputPath ("packet_stats/experiment4/packet_statisticsS" + run + ".txt",      filename_packet_stats,    sizeof (filename_packet_stats));
    OutputPath ("debug/debug_out.csv",                                             filename_debug,           sizeof (filename_debug));
    OutputPath ("positions/position_data.csv",                                     filename_positions,       sizeof (filename_positions));
    OutputPath ("flow_analysis/experiment3/flow_dataS" + run + ".csv",             filename_flow,            sizeof (filename_flow));
    OutputPath ("packet_stats/experiment4/packet_reassembleS" + run + ".csv",      filename_reassemble,      sizeof (filename_reassemble));
    OutputPath ("broadcast/experiment4/broadcast_statsS" + run + ".csv",           filename_broadcast_stats, sizeof (filename_broadcast_stats));
    OutputPath ("object_duplication/experiment4/obj_injectS" + run + ".csv",       filename_obj_inject,      sizeof (filename_obj_inject));
    OutputPath ("object_duplication/experiment4/obj_dupS" + run + ".csv",          filename_obj_dup,         sizeof (filename_obj_dup));

    sat_id = GetNode ()->GetId ();

    isRingUp    = m_topoConfig.ringUp   .count (sat_id) > 0;
    isRingDown  = m_topoConfig.ringDown .count (sat_id) > 0;
    isSeamLeft  = m_topoConfig.seamLeft .count (sat_id) > 0;
    isSeamRight = m_topoConfig.seamRight.count (sat_id) > 0;

    uint32_t rank = MpiInterface::GetSystemId ();

    // Initialise output files (rank-0 / sat-0 only)
    if (rank == 0 && sat_id == 0) {
        FILE* f_q   = fopen (filename_queue_stats,     "w");
        FILE* f_f   = fopen (filename_flow,            "w");
        FILE* f_p   = fopen (filename_packet_stats,    "w");
        FILE* f_d   = fopen (filename_debug,           "w");
        FILE* f_pos = fopen (filename_positions,       "w");
        FILE* f_r   = fopen (filename_reassemble,      "w");
        FILE* f_b   = fopen (filename_broadcast_stats, "w");
        FILE* f_oi = fopen (filename_obj_inject, "w");
        FILE* f_od = fopen (filename_obj_dup,    "w");

        if (f_q) { fprintf (f_q, "node,time,up,down,left,right,"
              "up_bc,down_bc,left_bc,right_bc,timestamp,queue\n"); fclose (f_q); }
        if (f_r) { fprintf (f_r, "node,time,type,origin,pkt_id\n"); fclose (f_r); }
        if (f_f) { fprintf (f_f, "node,time,out_up,out_down,out_left,out_right,in_up,in_down,in_left,"
                                 "in_right,drop_up,drop_right,drop_in_down,drop_in_left,final_drops\n"); fclose (f_f); }
        if (f_d) { fprintf (f_d, "node,time,rate_ratio,in_left,in_down,out_total,"
                                  "last_radj_sent,curr_radj_up,curr_radj_right\n"); fclose (f_d); }
        if (f_pos) { fprintf (f_pos, "node,time,x,y,z,v_x,v_y,v_z,is_eq\n"); fclose(f_pos);}
        if (f_b) { fprintf (f_b, "is_sender,node,origin,id,time\n"); fclose (f_b); }
        if (f_oi) { fprintf (f_oi, "time_s,origin,obj_id,num_frags,mode\n");          fclose (f_oi); }
        if (f_od) { fprintf (f_od, "time_s,relay_sat,origin,obj_id,frag_id\n");       fclose (f_od); }
        if (f_p) { fclose (f_p); }
    }

    RngSeedManager::SetSeed (rank * sat_id + sat_id - rank + 20 + m_runNumber);
    RngSeedManager::SetRun  (rank + 1);
    uniform_rnd = CreateObject<UniformRandomVariable> ();

    m_routing->Init (this);
    m_content->Init (this);
    // Register the routing strategy's OnReceive on all 4 directions
    auto cb = MakeCallback(&SatelliteForwardingApp::DispatchReceive, this);

    if (m_devUp)    m_devUp   ->SetReceiveCallback (cb);
    if (m_devDown)  m_devDown ->SetReceiveCallback (cb);
    if (m_devRight) m_devRight->SetReceiveCallback (cb);
    if (m_devLeft)  m_devLeft ->SetReceiveCallback (cb);

    Ptr<Queue<Packet>> q_up = DynamicCast<PointToPointLaserNetDevice> (m_devUp)->GetQueue ();
    Ptr<Queue<Packet>> q_down = DynamicCast<PointToPointLaserNetDevice> (m_devDown)->GetQueue ();
    Ptr<Queue<Packet>> q_left = DynamicCast<PointToPointLaserNetDevice> (m_devLeft)->GetQueue ();
    Ptr<Queue<Packet>> q_right = DynamicCast<PointToPointLaserNetDevice> (m_devRight)->GetQueue ();

    m_islQueueSize = q_up->GetMaxSize ().GetValue();
    std::string qStr = std::to_string (m_islQueueSize + QUEUE_BUFFER) + "p";
    for (Ptr<NetDevice> d : {m_devUp, m_devDown, m_devLeft, m_devRight})
    {
        Ptr<PointToPointLaserNetDevice> ld = DynamicCast<PointToPointLaserNetDevice> (d);
        ld->GetQueue ()         ->SetMaxSize (QueueSize (qStr));
        ld->GetBroadcastQueue ()->SetMaxSize (QueueSize (qStr));
    }

    current_packet_id = 0;

    NS_ASSERT_MSG(m_islQueueSize >= (obj_size / maxPayloadSize), "ERROR --- Objects do not fit in ISL queues --- ");

#if defined(MONITORE_PACKET) && (MONITORE_PACKET == 1)
    printf("ALARM: MONITORE_PACKET is enabled, simulation will run much slower!!!\n");
#endif

    // Monitoring
#if defined(MONITORE_GENERAL) && (MONITORE_GENERAL == 1)
    Simulator::Schedule (Seconds (0.01),  &SatelliteForwardingApp::MonitorQueues, this);
    Simulator::Schedule (Seconds (0.1),   &SatelliteForwardingApp::CheckPackets, this);
#endif

#if defined(MONITORE_POSITIONS) && (MONITORE_POSITIONS == 1)
    Simulator::Schedule (Seconds (0),   &SatelliteForwardingApp::MonitoreSatellitePositions, this);
#endif

    // Content generation
    m_content->Generate ();

    Simulator::Schedule(MilliSeconds(1), &SatelliteForwardingApp::AdjustQueueBuffer, this);

#if defined(DEBUG_CONFIG) && (DEBUG_CONFIG == 1)
    // Simulator::Schedule (MilliSeconds (1),  &SatelliteForwardingApp::ComputeDebugStatistics, this);
#endif

#if defined(MONITORE_FLOW_BALANCE) && (MONITORE_FLOW_BALANCE == 1)
    Simulator::Schedule(MilliSeconds(10), &SatelliteForwardingApp::MonitorFlowBalance, this);
#endif

}

void SatelliteForwardingApp::StopApplication (void) {}


// ─────────────────────────────────────────────────────────────────────────────
// Trampoline — routes the NS3 receive callback to the active strategy
// ─────────────────────────────────────────────────────────────────────────────
bool SatelliteForwardingApp::DispatchReceive (Ptr<NetDevice>    device,
                                              Ptr<const Packet> packet,
                                              uint16_t          protocol,
                                              const Address&    sender)
{
    return m_routing->OnReceive (device, packet, protocol, sender);
}

// ─────────────────────────────────────────────────────────────────────────────
// Shared helpers (called by routing strategies)
// ─────────────────────────────────────────────────────────────────────────────
void SatelliteForwardingApp::ForwardPacket (Ptr<NetDevice> outDev,
                                            Ptr<Packet>    pkt,
                                            bool           isUp)
{
    ForwardPacket (outDev, pkt, isUp, PROTO);
}

void SatelliteForwardingApp::ForwardPacket (Ptr<NetDevice> outDev,
                                            Ptr<Packet>    pkt,
                                            bool           isUp,
                                            const uint16_t proto)
{
    NS_ASSERT_MSG (outDev,
        "ForwardPacket: null device on sat " << sat_id
        << " (missing ISL link for this MPI rank?)");

    NS_ASSERT_MSG (outDev == m_devUp || outDev == m_devRight
                || outDev == m_devLeft || outDev == m_devDown,
                   "ERROR UNKNOWN DEV\n");

    Ptr<PointToPointLaserNetDevice> laserDev =
        DynamicCast<PointToPointLaserNetDevice> (outDev);
    NS_ASSERT_MSG (laserDev, "ForwardPacket: device is not a PointToPointLaserNetDevice");

    //
    // The queue we check and the protocol number we send with MUST agree:
    // Send() picks the queue from the protocol number, so checking one class's
    // depth and then enqueuing into the other lets that queue overflow silently.
    //
    Ptr<Queue<Packet>> q = (proto == PROTO_BROADCAST) ? laserDev->GetBroadcastQueue ()
                                       : laserDev->GetQueue ();

    if (q->GetCurrentSize ().GetValue () < q->GetMaxSize ().GetValue ())
    {
        outDev->Send (pkt, outDev->GetBroadcast (), proto);
        if (outDev == m_devUp)    m_bytesSentUp++;
        if (outDev == m_devRight) m_bytesSentRight++;
        if (outDev == m_devDown)  m_bytesSentDown++;
        if (outDev == m_devLeft)  m_bytesSentLeft++;
    }
    else
    {
        if (outDev == m_devUp)    m_dropsUp++;
        if (outDev == m_devRight) m_dropsRight++;
        num_dropped_packets++;
    }
}

void SatelliteForwardingApp::ResetPacketStatistics ()
{
    m_recentBytesInLeft   = 0;
    m_recentBytesInDown   = 0;
    m_recentBytesOutRight = 0;
    m_recentBytesOutUp    = 0;
    Simulator::Schedule (MilliSeconds (1), &SatelliteForwardingApp::ResetPacketStatistics, this);
}

void SatelliteForwardingApp::AdjustQueueBuffer()
{
    auto normalQ = [](Ptr<NetDevice> d) {
        return DynamicCast<PointToPointLaserNetDevice> (d)->GetQueue ();
    };
    auto bcastQ = [](Ptr<NetDevice> d) {
        return DynamicCast<PointToPointLaserNetDevice> (d)->GetBroadcastQueue ();
    };

    // Ring directions: grow to the ring buffer size if not already there.
    auto growTo = [](Ptr<Queue<Packet>> q, uint64_t target) {
        if (q->GetMaxSize ().GetValue () < target)
            q->SetMaxSize (QueueSize (std::to_string (target) + "p"));
    };

    // Non-ring directions: return to the base size once the backlog has
    // drained. The limit must NOT be set to the current backlog: the traffic
    // through a retired ring satellite is a saturated stream (in == out), so a
    // queue whose limit equals its occupancy drops every packet that arrives
    // just before a departure, i.e. the Doppler backlog of the old ring (and the
    // overlap absorbed during a switch) would be bled off as packet losses
    // instead of draining. Keep the enlarged limit until the queue has drained
    // back to the normal level (so the usual QUEUE_BUFFER headroom remains).
    auto shrinkToward = [this](Ptr<Queue<Packet>> q) {
        const uint64_t base      = m_islQueueSize + QUEUE_BUFFER;
        const uint64_t occupancy = q->GetCurrentSize ().GetValue ();
        if (q->GetMaxSize ().GetValue () > base && occupancy <= m_islQueueSize)
            q->SetMaxSize (QueueSize (std::to_string (base) + "p"));
    };

    // Do not give memory back while a ring switch is in progress: a satellite
    // that has just retired may still have to absorb the short overlap of the
    // old and the new stream (same window as the injection blackout).
    bool inSwitchWindow = false;
    const double now = Simulator::Now ().GetSeconds ();
    for (double t : switch_times)
    {
        if (now >= t - 1.0 && now <= t + SWITCH_SAFETY_TIMEOUT) { inSwitchWindow = true; break; }
    }

    if (isRingUp)
    {
        const uint64_t target = m_islQueueSize + QUEUE_BUFFER_RING;
        growTo (normalQ (m_devUp),   target);  growTo (bcastQ (m_devUp),   target);
        growTo (normalQ (m_devLeft), target);  growTo (bcastQ (m_devLeft), target);
    }
    else if (isRingDown)
    {
        const uint64_t target = m_islQueueSize + QUEUE_BUFFER_RING;
        growTo (normalQ (m_devDown),  target);  growTo (bcastQ (m_devDown),  target);
        growTo (normalQ (m_devRight), target);  growTo (bcastQ (m_devRight), target);
    }
    else if (!inSwitchWindow)
    {
        for (Ptr<NetDevice> d : {m_devUp, m_devDown, m_devLeft, m_devRight})
        {
            shrinkToward (normalQ (d));
            shrinkToward (bcastQ  (d));
        }
    }

    Simulator::Schedule (MilliSeconds (1), &SatelliteForwardingApp::AdjustQueueBuffer, this);
}

void SatelliteForwardingApp::GrowRingQueuesNow ()
{
    const uint64_t target = m_islQueueSize + QUEUE_BUFFER_RING;
    auto grow = [target](Ptr<NetDevice> d) {
        Ptr<PointToPointLaserNetDevice> l = DynamicCast<PointToPointLaserNetDevice> (d);
        for (Ptr<Queue<Packet>> q : {l->GetQueue (), l->GetBroadcastQueue ()})
            if (q->GetMaxSize ().GetValue () < target)
                q->SetMaxSize (QueueSize (std::to_string (target) + "p"));
    };
    if (isRingUp)        { grow (m_devUp);   grow (m_devLeft);  }
    else if (isRingDown) { grow (m_devDown); grow (m_devRight); }
}

// Packet tracking
uint32_t
SatelliteForwardingApp::AllocatePacketId ()
{
    double now = Simulator::Now ().GetSeconds ();
    m_records.push_back (PacketRecord{now, now});
    return current_packet_id++;
}

void
SatelliteForwardingApp::RecordReturn (PacketRecord* rec, bool up, double now)
{
    double& last = up ? rec->lastSeenUp : rec->lastSeenDown;
    if (last > 0)
    {
        m_rttSum += now - last;
        m_rttCount++;
    }
    last = now;
}

SatelliteForwardingApp::PacketRecord*
SatelliteForwardingApp::GetRecord (uint32_t id)
{
    if (id < m_baseId || (id - m_baseId) >= m_records.size ())
    {
        num_stale_records++; // Should not happen -> > 0 indicates failure
        return nullptr;
    }
    return &m_records[id - m_baseId];
}

void
SatelliteForwardingApp::EvictExpiredRecords ()
{
    double now = Simulator::Now ().GetSeconds ();

    // Ids are allocated in time order, so expired records form a prefix.
    while (!m_records.empty () && now - m_records.front ().created > TIME_TO_LIVE + 0.1)
    {
        const PacketRecord& r = m_records.front ();

        if (r.lastSeen > 0 && (now - r.lastSeen) > 3 + RTT_CORRECTION)
        {
            if (lost_packets_lookup.insert (m_baseId).second)
            {
                lost_packets.push (m_baseId);
            }
        }
        m_records.pop_front ();
        m_baseId++;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Monitoring
// ─────────────────────────────────────────────────────────────────────────────
void SatelliteForwardingApp::MonitorQueues (void)
{
    static time_t start_time = time (NULL);

    auto qN = [](Ptr<NetDevice> d) -> uint64_t {
        return DynamicCast<PointToPointLaserNetDevice> (d)->GetQueue ()
                 ->GetCurrentSize ().GetValue ();
    };
    auto qB = [](Ptr<NetDevice> d) -> uint64_t {
        return DynamicCast<PointToPointLaserNetDevice> (d)->GetBroadcastQueue ()
                 ->GetCurrentSize ().GetValue ();
    };

    FILE* f = fopen (filename_queue_stats, "a");
    if (f) {
        fprintf (f, "%u,%lf,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lf,%lu\n",
                 GetNode()->GetId (), Simulator::Now ().GetSeconds (),
                 qN(m_devUp), qN(m_devDown), qN(m_devLeft), qN(m_devRight),
                 qB(m_devUp), qB(m_devDown), qB(m_devLeft), qB(m_devRight),
                 (double)time (NULL) - start_time,
                 m_retryQueue.size ());
        fclose (f);
    }
    Simulator::Schedule (Seconds (0.01), &SatelliteForwardingApp::MonitorQueues, this);
}

void SatelliteForwardingApp::CheckPackets (void)
{
    EvictExpiredRecords ();

    double curr_time = Simulator::Now ().GetSeconds ();
    uint32_t in_system = 0, num_missing_packets = 0;

    for (const PacketRecord& r : m_records)
    {
        if (r.lastSeen <= 0) continue;                       // currently resent
        if (curr_time - r.lastSeen <= 6 + RTT_CORRECTION) in_system++;
        else                                                num_missing_packets++;
    }

    // curr_rtt = mean RTT over the returns since the previous stats line
    // (keeps the previous value if there was none in this interval)
    if (m_rttCount > 0)
    {
        curr_rtt   = m_rttSum / m_rttCount;
        m_rttSum   = 0.0;
        m_rttCount = 0;
    }

    FILE* f = fopen (filename_packet_stats, "a");
    if (f) {
        fprintf (f, "%u,%lf,%u,%u,%lf,%u,%u,%u\n",
                 GetNode()->GetId (), curr_time,
                 in_system, current_packet_id,
                 curr_rtt, num_dropped_packets,
                 num_missing_packets, num_stale_records);
        fclose (f);
    }

    if (current_packet_id != 0 && in_system != 0) {
        current_circulation_rate = (double)in_system / current_packet_id;
    }
    Simulator::Schedule (Seconds (0.01), &SatelliteForwardingApp::CheckPackets, this);
}

void SatelliteForwardingApp::ComputeDebugStatistics ()
{
    uint32_t bytesOut  = m_recentBytesOutRight + m_recentBytesOutUp;
    uint32_t bytesIn   = m_recentBytesInLeft   + m_recentBytesInDown;
    float traffic_ratio = (float)bytesIn / std::max ((float)bytesOut, 1.0f);
    (void)traffic_ratio; // suppress unused warning when commented out below

    FILE* f = fopen (filename_debug, "a");
    Ptr<PointToPointLaserNetDevice> laserDevUp =
        DynamicCast<PointToPointLaserNetDevice> (m_devUp);
    Ptr<PointToPointLaserNetDevice> laserDevRight =
        DynamicCast<PointToPointLaserNetDevice> (m_devRight);
    if (f) {
        fprintf (f, "%u,%lf %lu,%lu\n",
                 GetNode()->GetId (), Simulator::Now ().GetSeconds (),
                 laserDevUp->GetDataRate().GetBitRate(),laserDevRight->GetDataRate().GetBitRate());
        fclose (f);
    }
    Simulator::Schedule (MilliSeconds (10), &SatelliteForwardingApp::ComputeDebugStatistics, this);
}


void SatelliteForwardingApp::MonitorFlowBalance()
{
    FILE* f = fopen(filename_flow, "a");
    if (f != NULL){
        fprintf(f, "%u,%lf,"
                   "%u,%u,%u,%u,"  // bytes_sent per dir
                   "%u,%u,%u,%u,"  // bytes_recv per dir  
                   "%u,%u,"        // drops per dir
                   "%u,%u,"       // pkts_dropped from dir
                   "%u\n",
                sat_id, Now().GetSeconds(),
                m_bytesSentUp, m_bytesSentDown, m_bytesSentLeft, m_bytesSentRight,
                m_bytesRecvUp, m_bytesRecvDown, m_bytesRecvLeft, m_bytesRecvRight,
                m_dropsUp, m_dropsRight, m_dropsFromDown, m_dropsFromLeft,
                num_final_drops);
        fclose(f);
    }
    // Reset counters
    m_bytesSentUp = 0;
    m_bytesSentDown = 0;
    m_bytesSentLeft = 0;
    m_bytesSentRight = 0;
    m_bytesRecvUp = 0;
    m_bytesRecvDown = 0;
    m_bytesRecvLeft = 0;
    m_bytesRecvRight = 0;
    m_dropsUp = 0;
    m_dropsRight = 0;
    m_dropsFromDown = 0;
    m_dropsFromLeft = 0;

    Simulator::Schedule(MilliSeconds(10), 
        &SatelliteForwardingApp::MonitorFlowBalance, this);
}

void SatelliteForwardingApp::MonitoreSatellitePositions()
{

    Ptr<MobilityModel> mob = GetNode ()->GetObject<MobilityModel> ();
    Vector pos = mob->GetPosition ();
    Vector vel = mob->GetVelocity ();

    double lat = std::asin (pos.z / std::sqrt (pos.x*pos.x + pos.y*pos.y + pos.z*pos.z));

    double threshold = std::asin (std::sin((180.0 / m_satellitesPerOrbit) * (M_PI/180.0))
                                  * std::sin((double)m_inclination * (M_PI/180.0)));
    // printf("thresh: %lf lat: %lf\n", threshold, lat);
    // isCurrentlyEquator = (std::abs(lat) < std::abs(threshold)) ? 1 : 0;
    FILE* f = fopen (filename_positions, "a");
    if (f) {
        fprintf (f, "%u,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%u\n",
                 sat_id, Simulator::Now ().GetSeconds (),
                 pos.x, pos.y, pos.z, vel.x, vel.y, vel.z,isCurrentlyEquator);
        fclose (f);
    }
    Simulator::Schedule (Seconds (1), &SatelliteForwardingApp::MonitoreSatellitePositions, this);
}

// ─────────────────────────────────────────────────────────────────────────────
// Scheduler trampolines
// ─────────────────────────────────────────────────────────────────────────────
void SatelliteForwardingApp::TriggerRingUpSwitch ()
{
    // dynamic_cast returns null if backpressure wrapper is active or wrong routing type.
    if (auto* rs = dynamic_cast<RoutingRingSwitchStar*>(m_routing.get()))       rs->InitiateRingUpSwitch();
    else if (auto* rd = dynamic_cast<RoutingRingSwitchDelta*>(m_routing.get())) rd->InitiateRingUpSwitch();
    else NS_LOG_WARN("TriggerRingUpSwitch: sat " << sat_id << " does not use a ring-switch routing");
}

void SatelliteForwardingApp::TriggerRingDownSwitch ()
{
    if (auto* rs = dynamic_cast<RoutingRingSwitchStar*>(m_routing.get()))       rs->InitiateRingDownSwitch();
    else if (auto* rd = dynamic_cast<RoutingRingSwitchDelta*>(m_routing.get())) rd->InitiateRingDownSwitch();
    else NS_LOG_WARN("TriggerRingDownSwitch: sat " << sat_id << " does not use a ring-switch routing");
}

} // namespace ns3
