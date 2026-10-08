#ifndef SATELLITE_FORWARDING_APP_H
#define SATELLITE_FORWARDING_APP_H

#include "ns3/application.h"
#include "ns3/net-device.h"
#include "ns3/packet.h"
#include "ns3/address.h"
#include "ns3/header.h"
#include "ns3/random-variable-stream.h"
#include "ns3/rng-seed-manager.h"

#include "strategy-interfaces.h"
// Routing strategies
#include "routing/routing-ring-switch-walker-star.h"
#include "routing/routing-ring-switch-walker-delta.h"

// Content strategies
#include "content/content-fill-double.h"

#include "constellation-config.h"

#include <memory>
#include <vector>
#include <deque>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <cstddef>

// ── Strategy forward declarations ──────────────────────────────────────────
namespace ns3 {

class SatelliteForwardingApp;
class RingSwitchScheduler;

#define RTT_CORRECTION         0.2 // give n seconds extra time for the packet to come back
#define MONITORE_POSITIONS     0   // logs satellite position data in regular intervals
#define MONITORE_GENERAL       1   // enables queue and packet statistics
#define MONITORE_PACKET        0   // logs every packet hop (threshold, send-everywhere) WILL SLOW DOWN SIMULATION SIGNIFICANTLY
#define MONITORE_FLOW_BALANCE  0
#define DEBUG_CONFIG           0   
#define QUEUE_BUFFER           200
#define QUEUE_BUFFER_RING      100000 // ring satellites receive a larger buffer because of the doppler effect
#define GOSSIP_LOG_SAMPLE_RATE 10    // log first-reception for every N-th gossip broadcast (per origin)

// ── Backpressure tuning (edit here to change algorithm behaviour) ─────────────
// ISL links run at 80 Gbps — even 1 % rate reduction is ~800 Mbps, so keep gains small.
#define BP_CHECK_INTERVAL_MS  2        // how often each satellite checks its queues (ms)
#define BP_RELAY_MARGIN       0.05f    // relay signal if own queue > threshold*(1-this); default 5%
#define BP_RADJ_GAIN          1.0f     // radj = gain * overshoot_fraction  (linear, 1%→1%)
#define BP_MAX_RADJ           0.05f    // hard cap on rate reduction (5 % = 4 Gbps at 80 Gbps)
#define BP_RADJ_EPSILON       0.002f   // min radj change needed to emit a new signal (0.2 %)

// ── Ring-switch tuning ────────────────────────────────────────────────────────
#define RING_SWITCH_P           3      // packet type sentinel for ring-switch control packets
#define RING_SWITCH_DUMMY_P     4      // packet type for delay-dummy packets (silently dropped)
#define RING_SWITCH_UP_TIME_S   50.0   // simulation time (s) to fire the ring-up switch
#define RING_SWITCH_DOWN_TIME_S 50.0   // simulation time (s) to fire the ring-down switch

#define TIME_TO_LIVE_MS (TIME_TO_LIVE * 1000)


// ─────────────────────────────────────────────────────────────────────────────
// SatForwardHeader
// ─────────────────────────────────────────────────────────────────────────────
class SatForwardHeader : public Header
{
public:
    static TypeId GetTypeId (void);
    TypeId GetInstanceTypeId (void) const override;

    void Serialize   (Buffer::Iterator start) const override;
    uint32_t Deserialize (Buffer::Iterator start) override;
    void Print       (std::ostream& os) const override;
    uint32_t GetSerializedSize (void) const override { return 40; }

    void     SetN        (uint32_t n)  { m_n       = n;  }
    void     SetP        (uint32_t p)  { m_p       = p;  }
    void     SetId       (uint32_t id) { m_id      = id; }
    void     SetSat      (uint16_t s)  { m_sat     = s;  }
    void     SetCameLeft (uint16_t cl) { m_cameLeft= cl; }
    void     SetTime     (uint64_t t)  { m_time    = t;  }
    void     SetRadj     (float r)     { m_radj    = r;  }
    void     SetObjId    (uint32_t o)  { m_obj_id  = o;  }
    void     SetFragId   (uint32_t f)  { m_frag_id = f;  }
    void     SetDownEpoch (uint32_t e) { m_downEpoch = e; }

    int32_t  GetN        (void) const  { return m_n;       }
    uint32_t GetP        (void) const  { return m_p;       }
    uint32_t GetId       (void) const  { return m_id;      }
    uint16_t GetSat      (void) const  { return m_sat;     }
    uint16_t GetCameLeft (void) const  { return m_cameLeft;}
    uint64_t GetTime     (void) const  { return m_time;    }
    float    GetRadj     (void) const  { return m_radj;    }
    uint32_t GetObjId    (void) const  { return m_obj_id;  }
    uint32_t GetFragId   (void) const  { return m_frag_id; }
    uint32_t GetDownEpoch (void) const { return m_downEpoch; }

private:
    int32_t  m_n        {0};
    uint32_t m_p        {0};
    uint32_t m_id       {0};
    uint32_t m_obj_id   {0};
    uint32_t m_frag_id  {0};
    uint16_t m_sat      {0};
    uint16_t m_cameLeft {0};
    uint64_t m_time     {0};
    float    m_radj     {0.0f};
    uint32_t m_downEpoch {0};
};


// ─────────────────────────────────────────────────────────────────────────────
// SatelliteForwardingApp
// ─────────────────────────────────────────────────────────────────────────────
class SatelliteForwardingApp : public Application
{
public:
    static TypeId GetTypeId (void);
    SatelliteForwardingApp ();
    virtual ~SatelliteForwardingApp ();

    void SetupWithDevices (uint32_t       satellitesPerOrbit,
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
                           uint32_t       runNumber);

    // ── Accessors for strategies ──────────────────────────────────────────
    Ptr<NetDevice> GetDevUp    () const { return m_devUp; }
    Ptr<NetDevice> GetDevDown  () const { return m_devDown;  }
    Ptr<NetDevice> GetDevRight () const { return m_devRight; }
    Ptr<NetDevice> GetDevLeft  () const { return m_devLeft;  }
    uint32_t GetSatsPerOrbit   () const { return m_satellitesPerOrbit; }
    uint32_t GetNumSatellites  () const { return m_numSatellites; }
    uint16_t GetSatId          () const { return sat_id; }
    uint64_t GetIslQueueSize   () const { return m_islQueueSize; }
    const ConstellationConfig& GetTopoConfig () const { return m_topoConfig; }
    // Enlarge the ring queues of a ring member (isRingUp / isRingDown) now,
    // as AdjustQueueBuffer does on its next 1 ms tick. For the routing, when a
    // satellite becomes a ring member during a switch.
    void GrowRingQueuesNow ();
    uint64_t GetMaxIslQueueFillLevel() { return m_maxQueueFillLevel * (m_islQueueSize/100); }
    bool     GetUseBackpressure() const { return m_useBackpressure; }

    // Traffic counters — mutated by routing strategies
    uint32_t m_recentBytesInDown  {1};
    uint32_t m_recentBytesInLeft  {1};
    uint32_t m_recentBytesOutUp   {1};
    uint32_t m_recentBytesOutRight{1};

    // Flow monitoring
    uint32_t m_bytesSentUp {0};
    uint32_t m_bytesSentDown {0};
    uint32_t m_bytesSentLeft {0};
    uint32_t m_bytesSentRight {0};
    uint32_t m_bytesRecvUp {0};
    uint32_t m_bytesRecvDown {0};
    uint32_t m_bytesRecvLeft {0};
    uint32_t m_bytesRecvRight {0};

    uint32_t m_dropsUp {0};
    uint32_t m_dropsRight {0};

    uint32_t m_dropsFromDown {0};
    uint32_t m_dropsFromLeft {0};

    uint32_t m_runNumber{0};

    // Packet tracking — mutated by content/routing strategies
    struct PacketRecord {
        double   created  {0.0};   // fixed at allocation; drives eviction
        double   lastSeen {0.0};   // updated on each return; <=0 means "resent"
        double   lastSeenUp   {0.0};   // last return of the UP copy   (0 = none yet), RTT only
        double   lastSeenDown {0.0};   // last return of the DOWN copy (0 = none yet), RTT only
    };

    // RTT statistics. The routing calls RecordReturn whenever an own UP or DOWN
    // copy is back at its origin. RTT = time between two returns of the SAME
    // copy (UP and DOWN tracked separately); CheckPackets writes the mean over
    // all returns since the previous stats line as curr_rtt.
    void          RecordReturn (PacketRecord* rec, bool up, double now);

    // Allocates the next id and its record together, so the two can never
    // drift apart 
    uint32_t      AllocatePacketId ();
    PacketRecord* GetRecord (uint32_t id); // nullptr if evicted
    size_t        GetNumLiveRecords () const { return m_records.size (); } // own packets not yet evicted (TTL)
    void          EvictExpiredRecords ();
    uint64_t num_stale_records {0};        // arrivals past the horizon: should stay 0

    // Packet stats
    std::queue<uint32_t> lost_packets; 
    std::unordered_set<uint32_t> lost_packets_lookup;
    uint32_t current_packet_id  {0};
    double   curr_rtt           {10.0};  // mean RTT of the last stats interval (10.0 = none measured yet)
    double   m_rttSum           {0.0};   // RTTs measured since the last stats line
    uint64_t m_rttCount         {0};
    uint32_t num_dropped_packets{0};
    double   current_circulation_rate{1.0};

    // Object tracking
    std::deque<std::pair<uint32_t,uint32_t>> pending_objects;
    uint32_t m_numPendingPackets{0};
    bool     stop_generating_objects{false};
    uint32_t current_obj_id{0};
    uint32_t obj_size{15000}; // CHANGE -> {10, 100, 666} packets

    // Retry queue used by routing strategies
    std::queue<std::pair<Ptr<NetDevice>, Ptr<Packet>>> m_retryQueue;
    bool m_retryScheduled{false};

    static constexpr uint16_t PROTO           = 0x0800;
    static constexpr uint16_t PROTO_BROADCAST = 0x0801;
    static constexpr uint32_t maxPayloadSize  = 1500;

    // Shared helpers called by strategies
    void ForwardPacket (Ptr<NetDevice> outDev, Ptr<Packet> pkt, bool isUp);
    void ForwardPacket (Ptr<NetDevice> outDev, Ptr<Packet> pkt, bool isUp,
                        const uint16_t proto);

    // Monitoring
    void MonitorQueues         ();
    void CheckPackets          ();
    void ComputeDebugStatistics();
    void MonitoreSatellitePositions();
    void MonitorFlowBalance();

    // ── Output files ──────────────────────────────────────────────────────────
    // Every statistics / debug file is written below ONE output folder. The
    // main program sets it once, before the simulation starts
    // (storage_in_space --outDir=<folder>). Default: "mysim_results", relative
    // to the folder the simulation runs in (a relative folder is resolved
    // against the working directory, an absolute one is used as is).
    // Below it the layout is fixed, e.g. queue_stats/experiment4/....
    static void        SetOutputDir (const std::string& dir);
    static std::string GetOutputDir ();
    // "<output folder>/<relPath>"; missing parent folders are created.
    static std::string OutputPath   (const std::string& relPath);
    // Same, written into one of the fixed-size filename buffers below; stops
    // the simulation with a clear message if the path does not fit.
    static void        OutputPath   (const std::string& relPath, char* dst, std::size_t dstSize);

    // File paths (set once in StartApplication via OutputPath, read by stats)
    char filename_queue_stats[512];
    char filename_packet_stats[512];
    char filename_debug[512];
    char filename_positions[512];
    char filename_flow[512];
    char filename_reassemble[512];
    char filename_broadcast_stats[512];
    char filename_obj_inject[512];
    char filename_obj_dup[512];

    Ptr<UniformRandomVariable> uniform_rnd;
    bool isCurrentlyEquator{false};

    bool isSeamLeft {false};
    bool isSeamRight{false};
    bool isRingUp   {false};
    bool isRingDown {false};
    // Set by the routing: no new copies (admission, second copies) into the
    // UP / DOWN queue of this satellite. Used by the Walker-Delta routing for
    // the exit of the kink orbit, whose UP / DOWN link carries no ring traffic
    // (copies sent there would merge with the ring stream one hop later).
    bool m_noInjectUp   {false};
    bool m_noInjectDown {false};

    // Ring-switch: set for the two satellites that bracket the next ring position.
    // isNewSeamRight* = initiator satellite (sends first switch packet, becomes new seamRight).
    // isNewSeamLeft*  = terminus satellite (last in the wave, becomes new seamLeft).
    bool isNewSeamRightUp  {false};
    bool isNewSeamLeftUp   {false};
    bool isNewSeamRightDown{false};
    bool isNewSeamLeftDown {false};

    bool isGateway{false};

    // Pointer wired by RingSwitchScheduler after all apps are created.
    // Null when the scheduler is not in use (static timing or MPI mode).
    RingSwitchScheduler* m_ringScheduler {nullptr};

    std::vector<double> switch_times;

    // Trampoline called by RingSwitchScheduler to fire a ring switch on this satellite.
    void TriggerRingUpSwitch   ();
    void TriggerRingDownSwitch ();

    void AdjustQueueBuffer(void);

    float m_inclination {86.4};
    uint32_t       m_numOrbits         {0};
    uint32_t m_maxQueueFillLevel {1};

    uint32_t num_final_drops {0};

    uint32_t m_currUpEpoch{0};
    uint32_t m_currDownEpoch{0};

private:
    virtual void StartApplication (void) override;
    virtual void StopApplication  (void) override;

    void  ResetPacketStatistics (void);

    void SendLostPackets(void);


    bool DispatchReceive (Ptr<NetDevice>    device,
                          Ptr<const Packet> packet,
                          uint16_t          protocol,
                          const Address&    sender);


    // Strategy instances
    std::unique_ptr<RoutingStrategy>  m_routing;
    std::unique_ptr<ContentStrategy>  m_content;
    // std::unique_ptr<Statistics>       m_statistics;

    // Devices
    uint32_t       m_satellitesPerOrbit{0};
    uint32_t       m_numSatellites     {0};
    Ptr<NetDevice> m_devUp             {nullptr};
    Ptr<NetDevice> m_devDown           {nullptr};
    Ptr<NetDevice> m_devRight          {nullptr};
    Ptr<NetDevice> m_devLeft           {nullptr};
    bool                m_injectSeed        {false};
    bool                m_useBackpressure   {false};
    ConstellationConfig m_topoConfig        {};
    uint32_t m_trafficShare {2};

    uint16_t sat_id{0};
    uint64_t m_islQueueSize{0};

    std::deque<PacketRecord> m_records;
    uint32_t m_baseId {0};            // id of m_records.front()
};

} // namespace ns3
#endif // SATELLITE_FORWARDING_APP_H
