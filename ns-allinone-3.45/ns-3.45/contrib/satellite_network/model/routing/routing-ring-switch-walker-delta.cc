#include "routing-ring-switch-walker-delta.h"
#include "../ring-switch-scheduler.h"
#include "../satellite-forwarding-app.h"

#include "ns3/simulator.h"
#include "ns3/mobility-model.h"
#include "ns3/queue.h"
#include "ns3/point-to-point-laser-net-device.h"
#include "ns3/node.h"
#include "ns3/node-list.h"
#include "ns3/channel.h"

#include <cmath>
#include <cstdint>
#include <array>
#include <vector>
#include <queue>
#include <limits>
#include <functional>
#include <algorithm>
#include <string>

namespace ns3 {

// ─────────────────────────────────────────────────────────────────────────────
// Overview – storage rings for Walker-Delta constellations (e.g. Starlink)
//
// 1. TOPOLOGY (sat id = orbit * No + position, No = satellites per orbit)
//    devUp / devDown : next / previous satellite in the orbit (position +1/-1)
//    devRight/devLeft: satellite in the next / previous orbit. The inter-orbit
//    ISLs connect equal positions, except between the last and the first
//    orbit, where Walker phasing shifts them by one: (O-1, p) -> (0, p+1).
//    No seam: the inter-orbit dimension is a cycle, both rings are CLOSED loops
//    (writeup Table 1: torus), there is no return stream.
//
// 2. RINGS
//    UP copies travel upwards in their orbit, DOWN copies downwards. Each copy
//    goes once around its orbit and then leaves it at the orbit's EXIT:
//      UP  : exit forwards to the right (devDown -> devRight),
//      DOWN: exit forwards to the left  (devUp   -> devLeft).
//    The copy arrives in the next orbit at that orbit's ENTRY and is put into
//    the orbit (UP: devLeft -> devUp, DOWN: devRight -> devDown).
//    The exits of all orbits form the ring row: one satellite per orbit, all
//    at the same position (constellation config, ringUp / ringDown). The entry
//    of an orbit is the ISL neighbour of the previous exit; it equals the exit
//    in every orbit except one (kink orbit), because of the one-position shift
//    between the last and the first orbit:
//      UP  : kink in orbit 0,     entry = exit + 1 (lap of No-1 hops)
//      DOWN: kink in orbit O-1,   entry = exit - 1 (lap of No-1 hops)
//    Kink entries are ring members (enlarged queues) but never exit traffic.
//    The kink exit's UP (DOWN) link leads to the kink entry and carries no
//    ring traffic: copies the kink exit would add there (admission, second
//    copies) would merge with the ring stream at the entry and overflow its
//    queue. The kink exit therefore admits no copies in that direction
//    (SatelliteForwardingApp::m_noInjectUp / m_noInjectDown).
//
// 3. ROUTING RULES (RouteUp / RouteDown), the same for every satellite
//      UP  : devLeft -> devUp (enter; also late traffic of a retired ring)
//            devDown -> devRight if exit, otherwise devUp (pass on)
//      DOWN: devRight -> devDown (enter)
//            devUp -> devLeft if this satellite is the exit of the copy's
//            epoch, otherwise devDown (pass on)
//    Every other port is unexpected and counted as a drop (LogUnknownDevice).
//    An exit fills its ring ISL queue with dummy packets up to the ISL queue
//    size when it starts exiting (UP: role activation, DOWN: first copy of a
//    new epoch), so the new ring ISL has the queueing delay of the old one.
//
// 4. EPOCHS
//    Epoch e = number of switches; both rings move from position n to n-1
//    every switch (RingSwitchScheduler), the exit of orbit k at epoch e is
//    (exit0[k] - e) mod No.
//
// 5. RING SWITCH (writeup 2.4, Walker-Delta variant). The switch is started
//    in the kink orbit by the scheduler (InitiateRingUp/DownSwitch on the new
//    exit of the kink orbit). UP and DOWN need different mechanisms, because
//    in the UP ring the new exit lies one hop BEFORE the old one on the lap
//    and in the DOWN ring one hop AFTER it:
//      UP (role wave): a satellite takes its role (exit / entry / none) of
//         epoch e when it first sees a copy with epoch e (any port), UP copies
//         leaving over the ring ISL carry the exit's epoch. The new exit of
//         orbit k sees epoch e first on devLeft (from the new exit of orbit
//         k-1) and exits the lap traffic arriving from below from then on;
//         the old exit one position above stops when it sees epoch e from
//         below. Copies in flight do one hop less in one orbit, once. The
//         wave moves one ISL crossing per orbit (fast) and closes in the kink
//         orbit, where the old exit becomes the new entry.
//      DOWN (per-copy): a role wave would make the new exit (one hop after
//         the old entry) exit the copies that have just entered: every orbit
//         would send out its lap content twice as fast for one lap and the
//         entry queue of the next orbit would overflow. Instead every DOWN
//         copy exits at the exit of the epoch it carries, so copies that
//         entered at the old entry make their full lap to the old exit, and
//         copies of the new epoch enter at the new entry and exit at the new
//         exit. The initiator (new exit = old entry of the kink orbit) gives
//         the new epoch to every copy that enters there (devRight); these
//         make one hop more in the kink orbit, once, over the link from the
//         old exit, which carried no ring traffic before. The new epoch then
//         spreads with the copies, one lap per orbit (one ring revolution in
//         total), and the old exits stop when their last copy has passed.
//      Late copies that still arrive at a retired satellite on the ring ports
//      enter the orbit (rule 3); they are never lost.
//
// 6. DOWN QUEUE REGULATION (as [DOWN-REGULATION] in the Walker-Star routing)
//    At a DOWN switch the old entry of every orbit gets no DOWN traffic for
//    one lap (its last copies have entered, the new-epoch copies arrive only
//    after their lap), so its DOWN queue runs empty; without compensation its
//    content ends up in the new entry's queue, and with every switch one more
//    row of DOWN queues would be empty and the entry queues would grow. From
//    the end of the fill phase on, every satellite therefore tops its DOWN
//    queue up with dummies to its level at the end of the fill phase
//    (at most the ISL queue size) before it queues a DOWN copy
//    (RegulateDownQueue). Only queueing delay is restored, no stored copy is
//    created or lost; queues above that level are never touched. The dummies
//    also delay the new-epoch stream by that queue level, which gives the next
//    orbit's new entry the time to take over without an overlap.
//
// BROADCAST:
//   Three interchangeable algorithms, selected by kBroadcastMode below:
//     BCAST_FLOOD    — epidemic flooding, forward to all neighbours except the
//                      incoming one. ~2161 transmissions per broadcast on a
//                      36x20 grid, redundancy 3.01x. Delay-optimal (it is a
//                      BFS by delay) and fault tolerant, but wasteful.
//     BCAST_DIMORDER — dimension-ordered (YX) spanning tree carried by a hop
//                      counter in last_hop. Exactly N-1 = 719 transmissions,
//                      zero duplicates, but ~45 ms slower than flooding
//                      because it pins every cross-plane hop to the source's
//                      latitude, where the ISLs may be long.
//     BCAST_SPT      — delay-weighted shortest-path tree, see below. Also
//                      719 transmissions and zero duplicates, AND matches
//                      flooding's completion time exactly. Default.
// ─────────────────────────────────────────────────────────────────────────────

NS_OBJECT_ENSURE_REGISTERED (RingSwitchDeltaHeader);

TypeId RingSwitchDeltaHeader::GetTypeId (void)
{
    static TypeId tid = TypeId ("ns3::RingSwitchDeltaHeader")
        .SetParent<Header> ()
        .SetGroupName ("SatelliteNetwork")
        .AddConstructor<RingSwitchDeltaHeader> ();
    return tid;
}

TypeId RingSwitchDeltaHeader::GetInstanceTypeId (void) const { return GetTypeId (); }

void RingSwitchDeltaHeader::Serialize (Buffer::Iterator start) const
{
    start.WriteHtonU32 (m_id);
    start.WriteHtonU16 (m_sat);
    start.WriteHtonU64 (m_time);
    start.WriteHtonU64 (m_ttl);
    start.WriteHtonU16 (m_direction);
    start.WriteHtonU32 (m_epoch);
    start.WriteHtonU32 (m_obj_id);
    start.WriteHtonU32 (m_frag_id);
    start.WriteHtonU32 (m_last_hop);
    start.WriteHtonU16 (m_dup_code);
}

uint32_t RingSwitchDeltaHeader::Deserialize (Buffer::Iterator start)
{
    m_id       = start.ReadNtohU32 ();
    m_sat      = start.ReadNtohU16 ();
    m_time     = start.ReadNtohU64 ();
    m_ttl      = start.ReadNtohU64 ();
    m_direction= start.ReadNtohU16 ();
    m_epoch    = start.ReadNtohU32 ();
    m_obj_id   = start.ReadNtohU32 ();
    m_frag_id  = start.ReadNtohU32 ();
    m_last_hop = start.ReadNtohU32 ();
    m_dup_code = start.ReadNtohU16 ();
    return 42;
}

void RingSwitchDeltaHeader::Print (std::ostream& os) const
{
    os << "RingSwitchDeltaHeader dir=" << m_direction
       << " id=" << m_id
       << " time=" << m_time
       << " sat=" << m_sat;
}


// ═════════════════════════════════════════════════════════════════════════════
// BROADCAST: mode selection + delay-weighted shortest-path-tree machinery
//
// Everything here is file-local, so no header changes are required.
//
// The tree is rebuilt every kTreeRebuildPeriod seconds from live MobilityModel
// positions. Link delay = geometric distance / c. One Dijkstra per source over
// ~720 nodes takes a few hundred microseconds, so a full rebuild is single-digit
// milliseconds; the geometry changes on a ~48 minute timescale, so 10 s is very
// conservative.
//
// Forwarding is then a bitmask lookup: child[me][src] says which of the four
// devices are this satellite's children in the tree rooted at src.
//
// Walker DELTA: the plane dimension wraps all the way round, so there is no
// topology seam and all four ISLs are always usable. No latitude cutoff is
// applied - cross-orbit ISLs are used at every latitude, including over the
// poles where they are shortest (~475 km vs ~1234 km at the equator).
// ═════════════════════════════════════════════════════════════════════════════

namespace {   // file-local: no header changes needed for any of this

// ── Which broadcast algorithm to run ────────────────────────────────────────
enum BroadcastMode { BCAST_FLOOD = 1, BCAST_DIMORDER = 2, BCAST_SPT = 3,
                    BCAST_PRUNE = 4 };
constexpr BroadcastMode kBroadcastMode = BCAST_PRUNE;

// ── BCAST_PRUNE: distributed reverse-path pruning ───────────────────────────
// No topology database, no Dijkstra, no global state -- and therefore nothing
// that breaks under MPI, because every decision is made from packets that
// actually arrived on this satellite's own links.
//
// The insight is that no delay has to be measured: the FIRST copy of a packet
// to arrive came by the minimum-delay path, by definition. Arrival order is
// the measurement. So:
//
//   1. the first packet from source s floods normally;
//   2. a satellite that receives a DUPLICATE from s on link L sends
//      PRUNE(s) back down L;
//   3. on receiving PRUNE(s) on L, a satellite stops forwarding s-traffic
//      on L.
//
// After one flood per source every satellite has exactly one un-pruned
// incoming link -- its parent on the minimum-delay path -- so the surviving
// structure is the same delay-optimal tree the centralised Dijkstra produced,
// at the same N-1 transmissions. Convergence takes one flood, not N rounds.
//
// Prunes are soft state: they expire, the next packet re-floods, and the tree
// re-forms around the constellation's new geometry. This is essentially
// DVMRP / PIM-DM broadcast-and-prune, so the correctness argument is citable.
constexpr double kPruneLifetime = 30.0;   // s, prune expiry (soft state)
constexpr double kPruneRefresh  = 10.0;   // s, min gap between prunes on a link

// ── broadcast accounting (all modes) ────────────────────────────────────────
constexpr bool   kBcastStats       = true;
constexpr double kBcastStatsPeriod = 10.0;   // s

constexpr double   kC                 = 299792458.0;  // m/s
constexpr double   kFirstBuildDelay   = 0.1;          // s, let the topology settle
constexpr double   kTreeRebuildPeriod = 10.0;         // s, steady state
constexpr double   kTreeRetryPeriod   = 0.5;          // s, while the graph looks incomplete
constexpr uint32_t kMaxBuildRetries   = 200;
constexpr double   kInf               = std::numeric_limits<double>::infinity ();
constexpr uint8_t  DIR_UP = 0, DIR_DOWN = 1, DIR_LEFT = 2, DIR_RIGHT = 3;

// ── tree visualisation ──────────────────────────────────────────────────────
constexpr int  kTreeDumpSource    = 0;      // which source sat id to print; -1 = off
constexpr bool kTreeDumpGrid      = true;   // ASCII arrow grid to stdout
constexpr bool kTreeDumpPairs     = false;  // every parent->child pair to stdout
constexpr bool kTreeDumpCsv       = true;   // append the pairs to kTreeCsvRelPath
constexpr bool kTreeDumpAllToCsv  = false;  // ...for EVERY source
// relative to the output folder (SatelliteForwardingApp::OutputPath, --outDir)
const char* const kTreeCsvRelPath = "packet_stats/spt_tree.csv";
bool g_treeCsvInit = false;

// ─────────────────────────────────────────────────────────────────────────────
// Topology tables.
//
// IMPORTANT: satellites are identified by NODE, not by application. Under an
// MPI / distributed run every rank holds the full NodeList but installs
// applications only on its own satellites, so an app-based scan sees a single
// orbital plane and reports the other 700 as missing. Nodes, laser devices and
// mobility models exist on every rank, so building the graph from those yields
// the same tree everywhere -- which is exactly what a shared tree needs.
//
// Sat ids still matter because that is what the packet header carries. The
// nodeId -> satId offset is derived from whichever apps the rank owns and is
// checked for consistency.
// ─────────────────────────────────────────────────────────────────────────────
struct SptTopo
{
    double   lastBuild    = -1.0;
    bool     valid        = false;
    uint32_t n            = 0;   // number of satellites
    uint32_t nOrbits      = 0;
    uint32_t satsPerOrbit = 0;
    std::vector<uint32_t>                      satIdOf;   // idx   -> sat id
    std::vector<int32_t>                       idxOfSat;  // satId -> idx, -1 unknown
    std::vector<std::array<int32_t, 4>>        nbr;       // idx   -> peer idx per device slot
    std::vector<std::array<double, 4>>         w;         // one-way delay [s]
    std::vector<std::array<Ptr<NetDevice>, 4>> dev;       // the device in each slot
    std::vector<std::vector<double>>           dist;      // dist[src][idx]
    std::vector<std::vector<uint8_t>>          child;     // child[idx][src] = slot bitmask
};
SptTopo     g_spt;
uint64_t    g_sptDuplicates = 0;
bool        g_loopStarted   = false;
uint32_t    g_buildAttempts = 0;
std::string g_lastDiag;

// broadcast accounting, shared by all four modes
uint64_t g_txCopies = 0, g_firstRecv = 0, g_dupRecv = 0;
uint64_t g_pruneSent = 0, g_pruneRecv = 0;
uint32_t g_gridP = 0, g_gridS = 0;   // learned from the app, for the grid dump

SatelliteForwardingApp*
AppOf (Ptr<Node> node)
{
    if (!node) return nullptr;
    for (uint32_t i = 0; i < node->GetNApplications (); ++i)
    {
        SatelliteForwardingApp* a =
            dynamic_cast<SatelliteForwardingApp*> (PeekPointer (node->GetApplication (i)));
        if (a) return a;
    }
    return nullptr;
}

// A node counts as a satellite if it has at least two laser devices and a
// mobility model. No application required.
bool
SatDevices (Ptr<Node> node, std::array<Ptr<NetDevice>, 4>& out)
{
    out = {{nullptr, nullptr, nullptr, nullptr}};
    if (!node || !node->GetObject<MobilityModel> ()) return false;
    uint32_t cnt = 0;
    for (uint32_t i = 0; i < node->GetNDevices (); ++i)
    {
        Ptr<NetDevice> d = node->GetDevice (i);
        if (!DynamicCast<PointToPointLaserNetDevice> (d)) continue;
        if (cnt < 4) out[cnt] = d;
        ++cnt;
    }
    return cnt >= 2;
}

Ptr<Node>
PeerNode (Ptr<NetDevice> dev)
{
    if (!dev) return nullptr;
    Ptr<PointToPointLaserNetDevice> laser = DynamicCast<PointToPointLaserNetDevice> (dev);
    if (laser)
    {
        Ptr<Node> d = laser->GetDestinationNode ();
        if (d) return d;
    }
    Ptr<Channel> ch = dev->GetChannel ();
    if (!ch || ch->GetNDevices () != 2) return nullptr;
    for (std::size_t k = 0; k < ch->GetNDevices (); ++k)
    {
        Ptr<NetDevice> other = ch->GetDevice (k);
        if (other != dev) return other->GetNode ();
    }
    return nullptr;
}

// Device slot order is just NetDevice order and carries no UP/DOWN/LEFT/RIGHT
// meaning, so direction labels are derived from the (plane, slot) layout.
int8_t
DirFromIds (uint32_t fromSat, uint32_t toSat)
{
    const uint32_t S = g_spt.satsPerOrbit, P = g_spt.nOrbits;
    if (S == 0 || P == 0) return -1;
    uint32_t fp = fromSat / S, fs = fromSat % S, tp = toSat / S, ts = toSat % S;
    if (fp == tp)
    {
        if (ts == (fs + 1) % S) return (int8_t) DIR_UP;
        if (fs == (ts + 1) % S) return (int8_t) DIR_DOWN;
    }
    else if (fs == ts)
    {
        if (tp == (fp + 1) % P) return (int8_t) DIR_RIGHT;
        if (fp == (tp + 1) % P) return (int8_t) DIR_LEFT;
    }
    return -1;
}

const char* const kDirName[4] = { "UP", "DOWN", "LEFT", "RIGHT" };

// ─────────────────────────────────────────────────────────────────────────────
// Tree visualisation
// ─────────────────────────────────────────────────────────────────────────────
void
BuildParentArrays (uint32_t src,
                   std::vector<int32_t>&  parent,
                   std::vector<int8_t>&   dirToParent,
                   std::vector<uint32_t>& depth)
{
    const uint32_t n = g_spt.n;
    parent.assign (n, -1); dirToParent.assign (n, -1); depth.assign (n, 0);

    for (uint32_t v = 0; v < n; ++v)
        for (uint8_t d = 0; d < 4; ++d)
            if ((g_spt.child[v][src] >> d) & 1u)
            {
                int32_t u = g_spt.nbr[v][d];
                if (u >= 0) parent[u] = (int32_t) v;
            }

    for (uint32_t u = 0; u < n; ++u)
        if (parent[u] >= 0)
            dirToParent[u] = DirFromIds (g_spt.satIdOf[u], g_spt.satIdOf[parent[u]]);

    std::vector<uint32_t> q; q.push_back (src);
    std::vector<bool> seen (n, false); seen[src] = true;
    for (std::size_t i = 0; i < q.size (); ++i)
    {
        uint32_t v = q[i];
        for (uint8_t d = 0; d < 4; ++d)
            if ((g_spt.child[v][src] >> d) & 1u)
            {
                int32_t u = g_spt.nbr[v][d];
                if (u >= 0 && !seen[u])
                { seen[u] = true; depth[u] = depth[v] + 1; q.push_back ((uint32_t) u); }
            }
    }
}

inline bool
GridOk () { return g_spt.satsPerOrbit > 0 && g_spt.nOrbits > 0 &&
                   g_spt.nOrbits * g_spt.satsPerOrbit == g_spt.n; }

void
PrintTreeGrid (uint32_t src,
               const std::vector<int8_t>&   dirToParent,
               const std::vector<uint32_t>& depth)
{
    if (!GridOk ()) { printf ("[SPT] grid view unavailable (n=%u not a plane x slot grid)\n",
                              g_spt.n); return; }
    const uint32_t P = g_spt.nOrbits, S = g_spt.satsPerOrbit;
    static const char kArrow[4] = { '^', 'v', '<', '>' };
    uint32_t srcSat = g_spt.satIdOf[src];

    printf ("[SPT] tree for source sat %u (plane %u, slot %u) -- arrow points at the parent, "
            "packets flow against it; S = source, . = unreached\n",
            srcSat, srcSat / S, srcSat % S);

    printf ("      plane  ");
    for (uint32_t p = 0; p < P; ++p) printf ("%c", (p % 10 == 0) ? char ('0' + (p / 10) % 10) : ' ');
    printf ("\n             ");
    for (uint32_t p = 0; p < P; ++p) printf ("%u", p % 10);
    printf ("\n");

    for (uint32_t s = 0; s < S; ++s)
    {
        printf ("  slot %3u   ", s);
        for (uint32_t p = 0; p < P; ++p)
        {
            uint32_t sat = p * S + s;
            int32_t  idx = (sat < g_spt.idxOfSat.size ()) ? g_spt.idxOfSat[sat] : -1;
            char ch = '.';
            if (idx >= 0)
            {
                if ((uint32_t) idx == src)      ch = 'S';
                else if (dirToParent[idx] >= 0) ch = kArrow[dirToParent[idx]];
            }
            printf ("%c", ch);
        }
        printf ("\n");
    }

    uint32_t maxDepth = 0; double meanDepth = 0.0; uint32_t cnt = 0;
    for (uint32_t v = 0; v < g_spt.n; ++v)
        if (v != src && dirToParent[v] >= 0)
        { maxDepth = std::max (maxDepth, depth[v]); meanDepth += depth[v]; ++cnt; }
    if (cnt) meanDepth /= cnt;
    printf ("      depth: max %u hops, mean %.2f hops over %u satellites\n",
            maxDepth, meanDepth, cnt);
}

void
DumpTreePairs (uint32_t src,
               const std::vector<int32_t>&  parent,
               const std::vector<uint32_t>& depth,
               FILE* csv, bool toStdout)
{
    const uint32_t n = g_spt.n;
    const uint32_t S = GridOk () ? g_spt.satsPerOrbit : 0;
    if (toStdout) printf ("[SPT] parent -> child pairs for source sat %u\n", g_spt.satIdOf[src]);

    for (uint32_t u = 0; u < n; ++u)
    {
        if (u == src || parent[u] < 0) continue;
        uint32_t v  = (uint32_t) parent[u];
        uint32_t us = g_spt.satIdOf[u], vs = g_spt.satIdOf[v];

        int8_t slot = -1;
        for (uint8_t d = 0; d < 4; ++d)
            if (g_spt.nbr[v][d] == (int32_t) u) { slot = (int8_t) d; break; }
        int8_t txDir = DirFromIds (vs, us);
        double hopMs = (slot >= 0) ? g_spt.w[v][slot] * 1000.0 : 0.0;
        double cumMs = g_spt.dist[src][u] * 1000.0;
        const char* dn = (txDir >= 0) ? kDirName[txDir] : "?";

        if (toStdout)
        {
            if (S) printf ("        %4u (p%2u,s%2u) --%-5s--> %4u (p%2u,s%2u)  "
                           "%5.2f ms hop, depth %2u, %6.2f ms total\n",
                           vs, vs / S, vs % S, dn, us, us / S, us % S, hopMs, depth[u], cumMs);
            else   printf ("        %4u --%-5s--> %4u  %5.2f ms hop, depth %2u, %6.2f ms total\n",
                           vs, dn, us, hopMs, depth[u], cumMs);
        }
        if (csv)
            fprintf (csv, "%.6f,%u,%u,%u,%u,%u,%u,%u,%s,%.6f,%u,%.6f\n",
                     g_spt.lastBuild, g_spt.satIdOf[src], vs, us,
                     S ? vs / S : 0, S ? vs % S : 0, S ? us / S : 0, S ? us % S : 0,
                     dn, hopMs, depth[u], cumMs);
    }
}

void
DumpTrees ()
{
    if (kTreeDumpSource < 0 && !kTreeDumpAllToCsv) return;
    if (g_spt.n == 0) return;

    FILE* csv = nullptr;
    if (kTreeDumpCsv || kTreeDumpAllToCsv)
    {
        csv = fopen (SatelliteForwardingApp::OutputPath (kTreeCsvRelPath).c_str (), g_treeCsvInit ? "a" : "w");
        if (csv && !g_treeCsvInit)
        {
            fprintf (csv, "build_time,src,parent,child,parent_plane,parent_slot,"
                          "child_plane,child_slot,tx_dir,hop_delay_ms,depth,cum_delay_ms\n");
            g_treeCsvInit = true;
        }
    }

    std::vector<int32_t>  parent;
    std::vector<int8_t>   dirToParent;
    std::vector<uint32_t> depth;

    if (kTreeDumpSource >= 0 && (uint32_t) kTreeDumpSource < g_spt.idxOfSat.size ()
        && g_spt.idxOfSat[kTreeDumpSource] >= 0)
    {
        uint32_t src = (uint32_t) g_spt.idxOfSat[kTreeDumpSource];
        BuildParentArrays (src, parent, dirToParent, depth);
        if (kTreeDumpGrid) PrintTreeGrid (src, dirToParent, depth);
        if (kTreeDumpPairs || (csv && !kTreeDumpAllToCsv))
            DumpTreePairs (src, parent, depth, kTreeDumpAllToCsv ? nullptr : csv,
                           kTreeDumpPairs);
    }

    if (kTreeDumpAllToCsv && csv)
        for (uint32_t s = 0; s < g_spt.n; ++s)
        {
            BuildParentArrays (s, parent, dirToParent, depth);
            DumpTreePairs (s, parent, depth, csv, false);
        }

    if (csv) fclose (csv);
}

// ─────────────────────────────────────────────────────────────────────────────
// Rebuild the constellation graph and every source's shortest-path tree.
// ─────────────────────────────────────────────────────────────────────────────
void
RebuildSpt ()
{
    if (Simulator::Now ().GetSeconds () + 1e-9 < kFirstBuildDelay) return;

    // 1. every node that looks like a satellite -- by NODE, not by application
    std::vector<Ptr<Node>>                     nodes;
    std::vector<std::array<Ptr<NetDevice>, 4>> devs;
    std::vector<int32_t>                       idxOfNodeId;
    uint32_t nodesScanned = 0, appsFound = 0;

    for (auto it = NodeList::Begin (); it != NodeList::End (); ++it)
    {
        ++nodesScanned;
        std::array<Ptr<NetDevice>, 4> d;
        if (!SatDevices (*it, d)) continue;
        uint32_t nid = (*it)->GetId ();
        if (idxOfNodeId.size () <= nid) idxOfNodeId.resize (nid + 1, -1);
        idxOfNodeId[nid] = (int32_t) nodes.size ();
        nodes.push_back (*it);
        devs.push_back (d);
    }
    const uint32_t n = (uint32_t) nodes.size ();
    if (n == 0) return;

    // 2. nodeId -> satId offset, from whichever apps this rank owns
    bool    haveOffset = false, offsetOk = true;
    int64_t offset = 0;
    for (uint32_t i = 0; i < n; ++i)
    {
        SatelliteForwardingApp* a = AppOf (nodes[i]);
        if (!a) continue;
        ++appsFound;
        int64_t o = (int64_t) a->GetSatId () - (int64_t) nodes[i]->GetId ();
        if (!haveOffset) { offset = o; haveOffset = true; }
        else if (o != offset) offsetOk = false;
        if (g_spt.satsPerOrbit == 0)
        { g_spt.satsPerOrbit = a->GetSatsPerOrbit (); g_spt.nOrbits = a->m_numOrbits; }
    }

    g_spt.n = n;
    g_spt.satIdOf.assign (n, 0);
    uint32_t maxSat = 0;
    for (uint32_t i = 0; i < n; ++i)
    {
        int64_t sid = (int64_t) nodes[i]->GetId () + (haveOffset ? offset : 0);
        g_spt.satIdOf[i] = (uint32_t) std::max<int64_t> (sid, 0);
        maxSat = std::max (maxSat, g_spt.satIdOf[i]);
    }
    g_spt.idxOfSat.assign (maxSat + 1, -1);
    for (uint32_t i = 0; i < n; ++i) g_spt.idxOfSat[g_spt.satIdOf[i]] = (int32_t) i;

    // 3. adjacency + delays
    g_spt.nbr.assign (n, {{-1, -1, -1, -1}});
    g_spt.w.assign   (n, {{0.0, 0.0, 0.0, 0.0}});
    g_spt.dev.assign (n, {{nullptr, nullptr, nullptr, nullptr}});

    uint32_t devSlots = 0, devPresent = 0, peerFail = 0, noMobility = 0;
    for (uint32_t v = 0; v < n; ++v)
    {
        Vector pv = nodes[v]->GetObject<MobilityModel> ()->GetPosition ();
        for (uint8_t d = 0; d < 4; ++d)
        {
            ++devSlots;
            g_spt.dev[v][d] = devs[v][d];
            if (!devs[v][d]) continue;
            ++devPresent;
            Ptr<Node> peer = PeerNode (devs[v][d]);
            if (!peer) { ++peerFail; continue; }
            uint32_t pid = peer->GetId ();
            if (pid >= idxOfNodeId.size () || idxOfNodeId[pid] < 0) { ++peerFail; continue; }
            Ptr<MobilityModel> mu = peer->GetObject<MobilityModel> ();
            if (!mu) { ++noMobility; continue; }
            Vector pu = mu->GetPosition ();
            double dx = pv.x - pu.x, dy = pv.y - pu.y, dz = pv.z - pu.z;
            g_spt.nbr[v][d] = idxOfNodeId[pid];
            g_spt.w[v][d]   = std::sqrt (dx * dx + dy * dy + dz * dz) / kC;
        }
    }

    // 4. symmetrise
    for (uint32_t v = 0; v < n; ++v)
        for (uint8_t d = 0; d < 4; ++d)
        {
            int32_t u = g_spt.nbr[v][d];
            if (u < 0 || (uint32_t) u >= n) { g_spt.nbr[v][d] = -1; continue; }
            bool mutual = false;
            for (uint8_t k = 0; k < 4; ++k)
                if (g_spt.nbr[u][k] == (int32_t) v) { mutual = true; break; }
            if (!mutual) g_spt.nbr[v][d] = -1;
        }

    // 5. Dijkstra from every source
    g_spt.dist.assign (n, std::vector<double> (n, kInf));
    using QE = std::pair<double, uint32_t>;
    for (uint32_t s = 0; s < n; ++s)
    {
        std::vector<double>& D = g_spt.dist[s];
        D[s] = 0.0;
        std::priority_queue<QE, std::vector<QE>, std::greater<QE>> pq;
        pq.push (QE (0.0, s));
        while (!pq.empty ())
        {
            double du = pq.top ().first; uint32_t u = pq.top ().second; pq.pop ();
            if (du > D[u]) continue;
            for (uint8_t k = 0; k < 4; ++k)
            {
                int32_t v = g_spt.nbr[u][k];
                if (v < 0) continue;
                double nd = du + g_spt.w[u][k];
                if (nd < D[v] - 1e-15) { D[v] = nd; pq.push (QE (nd, (uint32_t) v)); }
            }
        }
    }

    // 6. child masks; ties broken by lowest sat id so every rank agrees
    g_spt.child.assign (n, std::vector<uint8_t> (n, 0));
    for (uint32_t s = 0; s < n; ++s)
    {
        const std::vector<double>& D = g_spt.dist[s];
        for (uint32_t u = 0; u < n; ++u)
        {
            if (u == s || D[u] == kInf) continue;
            int32_t best = -1; double bestCost = kInf;
            for (uint8_t k = 0; k < 4; ++k)
                if (g_spt.nbr[u][k] == (int32_t) s &&
                    std::fabs (g_spt.w[u][k] - D[u]) <= 1e-12)
                { best = (int32_t) s; break; }
            if (best < 0)
                for (uint8_t k = 0; k < 4; ++k)
                {
                    int32_t v = g_spt.nbr[u][k];
                    if (v < 0 || D[v] == kInf) continue;
                    double cost = D[v] + g_spt.w[u][k];
                    if (cost < bestCost - 1e-12 ||
                        (std::fabs (cost - bestCost) <= 1e-12 &&
                         (best < 0 || g_spt.satIdOf[v] < g_spt.satIdOf[best])))
                    { bestCost = cost; best = v; }
                }
            if (best < 0) continue;
            for (uint8_t k = 0; k < 4; ++k)
                if (g_spt.nbr[best][k] == (int32_t) u)
                { g_spt.child[best][s] |= (uint8_t) (1u << k); break; }
        }
    }

    // 7. sanity gate
    uint32_t edges = 0, reach = 0, treeTx = 0;
    for (uint32_t v = 0; v < n; ++v)
        for (uint8_t d = 0; d < 4; ++d) if (g_spt.nbr[v][d] >= 0) ++edges;
    for (uint32_t v = 0; v < n; ++v) if (g_spt.dist[0][v] != kInf) ++reach;
    for (uint32_t v = 0; v < n; ++v)
        for (uint8_t d = 0; d < 4; ++d) if ((g_spt.child[v][0] >> d) & 1u) ++treeTx;
    double worst = 0.0;
    for (uint32_t v = 0; v < n; ++v)
        if (g_spt.dist[0][v] != kInf) worst = std::max (worst, g_spt.dist[0][v]);

    ++g_buildAttempts;
    const bool sane = haveOffset && offsetOk && (peerFail == 0) && (noMobility == 0) &&
                      (reach == n) && (edges >= 2 * n);

    if (!sane)
    {
        g_spt.valid = false;
        char diag[512];
        snprintf (diag, sizeof (diag),
                  "nodesScanned=%u sats=%u appsFound=%u offset=%s devSlots=%u "
                  "devPresent=%u peerFail=%u noMobility=%u edges=%u reach=%u/%u",
                  nodesScanned, n, appsFound,
                  !haveOffset ? "NONE" : (offsetOk ? "ok" : "INCONSISTENT"),
                  devSlots, devPresent, peerFail, noMobility, edges, reach, n);
        if (g_lastDiag != diag && g_buildAttempts <= kMaxBuildRetries)
        {
            g_lastDiag = diag;
            printf ("[SPT] t=%.3fs  INCOMPLETE topology, not routing on it (attempt %u): "
                    "%s -- retrying in %.1fs\n",
                    Simulator::Now ().GetSeconds (), g_buildAttempts, diag, kTreeRetryPeriod);
        }
        return;
    }

    g_spt.lastBuild = Simulator::Now ().GetSeconds ();
    g_spt.valid     = true;

    printf ("[SPT] t=%.3fs  %u sats (%u local apps), %u directed links, src0: %u/%u reachable, "
            "%u tree transmissions (optimum %u), completion %.1f ms, dupHits=%lu\n",
            g_spt.lastBuild, n, appsFound, edges, reach, n, treeTx, n - 1,
            worst * 1000.0, (unsigned long) g_sptDuplicates);

    DumpTrees ();
}

void
SptRebuildLoop ()
{
    RebuildSpt ();
    Simulator::Schedule (Seconds (g_spt.valid ? kTreeRebuildPeriod : kTreeRetryPeriod),
                         &SptRebuildLoop);
}

// Forward to this satellite's children in the tree rooted at sat id `src`.
// Falls back to flooding while no valid tree exists, so broadcasts still
// complete during startup instead of vanishing.
void
SptForward (SatelliteForwardingApp* app, uint32_t src, Ptr<Packet> pkt,
            Ptr<NetDevice> inDev)
{
    int32_t meIdx = -1, srcIdx = -1;
    if (g_spt.valid)
    {
        uint32_t meSat = app->GetSatId ();
        if (meSat < g_spt.idxOfSat.size ()) meIdx  = g_spt.idxOfSat[meSat];
        if (src   < g_spt.idxOfSat.size ()) srcIdx = g_spt.idxOfSat[src];
    }

    if (!g_spt.valid || meIdx < 0 || srcIdx < 0)
    {
        Ptr<NetDevice> devs[4] = { app->GetDevUp (), app->GetDevDown (),
                                   app->GetDevLeft (), app->GetDevRight () };
        for (uint8_t d = 0; d < 4; ++d)
            if (devs[d] && devs[d] != inDev)
                app->ForwardPacket (devs[d], pkt->Copy (), true,
                                    SatelliteForwardingApp::PROTO_BROADCAST);
        return;
    }

    uint8_t mask = g_spt.child[meIdx][srcIdx];
    if (mask == 0) return;
    for (uint8_t d = 0; d < 4; ++d)
        if (((mask >> d) & 1u) && g_spt.dev[meIdx][d])
        {
            ++g_txCopies;
            app->ForwardPacket (g_spt.dev[meIdx][d], pkt->Copy (), true,
                                SatelliteForwardingApp::PROTO_BROADCAST);
        }
}


// ═════════════════════════════════════════════════════════════════════════════
// BCAST_PRUNE state and helpers
//
// Per-satellite, per-source, per-link soft state. Kept file-local and indexed
// by sat id so no header change is needed; under MPI only the rank's own
// satellites ever allocate a row.
// ═════════════════════════════════════════════════════════════════════════════
std::vector<std::vector<std::array<float, 4>>> g_pruneUntil;   // [me][src][slot]
std::vector<std::vector<std::array<float, 4>>> g_pruneSentAt;  // [me][src][slot]
std::vector<std::vector<int32_t>>              g_parentSat;    // [me][src] = parent sat id

int
DevSlot (SatelliteForwardingApp* app, Ptr<NetDevice> d)
{
    if (!d) return -1;
    if (d == app->GetDevUp ())    return 0;
    if (d == app->GetDevDown ())  return 1;
    if (d == app->GetDevLeft ())  return 2;
    if (d == app->GetDevRight ()) return 3;
    return -1;
}

void
EnsurePruneState (uint32_t me, uint32_t src)
{
    if (g_pruneUntil.size () <= me)
    {
        g_pruneUntil.resize (me + 1);
        g_pruneSentAt.resize (me + 1);
        g_parentSat.resize (me + 1);
    }
    if (g_pruneUntil[me].size () <= src)
    {
        g_pruneUntil[me].resize  (src + 1, {{0.0f, 0.0f, 0.0f, 0.0f}});
        g_pruneSentAt[me].resize (src + 1, {{-1e9f, -1e9f, -1e9f, -1e9f}});
        g_parentSat[me].resize   (src + 1, -1);
    }
}

bool
IsPruned (SatelliteForwardingApp* app, uint32_t src, Ptr<NetDevice> d)
{
    int k = DevSlot (app, d);
    if (k < 0) return false;
    uint32_t me = app->GetSatId ();
    EnsurePruneState (me, src);
    return Simulator::Now ().GetSeconds () < (double) g_pruneUntil[me][src][k];
}

// Tell the upstream neighbour to stop sending source `src` down this link.
void
SendPrune (SatelliteForwardingApp* app, uint32_t src, Ptr<NetDevice> d)
{
    int k = DevSlot (app, d);
    if (k < 0) return;
    uint32_t me = app->GetSatId ();
    EnsurePruneState (me, src);

    double now = Simulator::Now ().GetSeconds ();
    if (now < (double) g_pruneSentAt[me][src][k] + kPruneRefresh) return;  // rate limit
    g_pruneSentAt[me][src][k] = (float) now;

    Ptr<Packet> p = Create<Packet> (0);          // header only, ~42 B on the wire
    RingSwitchDeltaHeader h;
    h.SetId        (0);
    h.SetSat       ((uint16_t) src);
    h.SetTime      (Simulator::Now ().GetMilliSeconds ());
    h.SetTTL       (10);
    h.SetDirection (BROADCAST_PRUNE);
    h.SetEpoch     (0);
    h.SetObjId     (0);
    h.SetFragId    (0);
    h.SetLastHop   (me);
    h.SetDupCode   (0);
    p->AddHeader (h);

    ++g_pruneSent;
    app->ForwardPacket (d, p, true, SatelliteForwardingApp::PROTO_BROADCAST);
}

void
ApplyPrune (SatelliteForwardingApp* app, uint32_t src, Ptr<NetDevice> d)
{
    int k = DevSlot (app, d);
    if (k < 0) return;
    uint32_t me = app->GetSatId ();
    EnsurePruneState (me, src);
    g_pruneUntil[me][src][k] =
        (float) (Simulator::Now ().GetSeconds () + kPruneLifetime);
    ++g_pruneRecv;
}

// The link the first copy arrived on IS the minimum-delay parent.
void
RecordParent (SatelliteForwardingApp* app, uint32_t src, uint32_t parentSat)
{
    uint32_t me = app->GetSatId ();
    EnsurePruneState (me, src);
    g_parentSat[me][src] = (int32_t) parentSat;
    if (g_gridS == 0) { g_gridS = app->GetSatsPerOrbit (); g_gridP = app->m_numOrbits; }
}

// ─────────────────────────────────────────────────────────────────────────────
// Periodic accounting, and for BCAST_PRUNE an arrow grid of the tree that the
// prunes actually carved out -- built from observed arrivals, not from a
// topology database. Under MPI each rank prints its own satellites; cells it
// does not own show as '.'.
// ─────────────────────────────────────────────────────────────────────────────
void
BcastStatsLoop ()
{
    static uint64_t lastTx = 0, lastFirst = 0, lastDup = 0, lastPs = 0, lastPr = 0;
    uint64_t tx = g_txCopies - lastTx,   fr = g_firstRecv - lastFirst;
    uint64_t du = g_dupRecv  - lastDup,  ps = g_pruneSent - lastPs;
    uint64_t pr = g_pruneRecv - lastPr;
    lastTx = g_txCopies; lastFirst = g_firstRecv; lastDup = g_dupRecv;
    lastPs = g_pruneSent; lastPr = g_pruneRecv;

    const char* mode = (kBroadcastMode == BCAST_FLOOD)    ? "FLOOD"
                     : (kBroadcastMode == BCAST_DIMORDER) ? "DIMORDER"
                     : (kBroadcastMode == BCAST_SPT)      ? "SPT" : "PRUNE";

    if (fr > 0)
        printf ("[BCAST %s] t=%.1fs  window: %lu tx, %lu first-recv, %lu dup, "
                "%.2f tx per delivery, prunes sent/recv %lu/%lu\n",
                mode, Simulator::Now ().GetSeconds (),
                (unsigned long) tx, (unsigned long) fr, (unsigned long) du,
                (double) tx / (double) fr,
                (unsigned long) ps, (unsigned long) pr);

    if (kBroadcastMode == BCAST_PRUNE && kTreeDumpGrid && kTreeDumpSource >= 0
        && g_gridS > 0 && g_gridP > 0)
    {
        uint32_t src = (uint32_t) kTreeDumpSource;
        static const char kArrow[4] = { '^', 'v', '<', '>' };
        // reuse DirFromIds, which needs the grid dimensions in g_spt
        g_spt.satsPerOrbit = g_gridS; g_spt.nOrbits = g_gridP;

        printf ("[BCAST PRUNE] pruned tree for source sat %u -- arrow points at the "
                "parent (the link the first copy arrived on); S = source, . = unknown\n", src);
        printf ("      plane  ");
        for (uint32_t pl = 0; pl < g_gridP; ++pl)
            printf ("%c", (pl % 10 == 0) ? char ('0' + (pl / 10) % 10) : ' ');
        printf ("\n             ");
        for (uint32_t pl = 0; pl < g_gridP; ++pl) printf ("%u", pl % 10);
        printf ("\n");
        uint32_t known = 0;
        for (uint32_t sl = 0; sl < g_gridS; ++sl)
        {
            printf ("  slot %3u   ", sl);
            for (uint32_t pl = 0; pl < g_gridP; ++pl)
            {
                uint32_t sat = pl * g_gridS + sl;
                char ch = '.';
                if (sat == src) ch = 'S';
                else if (sat < g_parentSat.size () && src < g_parentSat[sat].size ()
                         && g_parentSat[sat][src] >= 0)
                {
                    int8_t dir = DirFromIds (sat, (uint32_t) g_parentSat[sat][src]);
                    if (dir >= 0) { ch = kArrow[dir]; ++known; }
                }
                printf ("%c", ch);
            }
            printf ("\n");
        }
        printf ("      %u/%u parents known on this rank\n", known, g_gridP * g_gridS);
    }

    Simulator::Schedule (Seconds (kBcastStatsPeriod), &BcastStatsLoop);
}

} // anonymous namespace



// bool
// RoutingRingSwitchDelta::SeenBefore (uint64_t key, uint64_t now_ms)
// {
    // // Everything in m_seenPrev was inserted at least TIME_TO_LIVE_MS ago, so
    // // any surviving copy would fail the TTL check before reaching us.
    // if (now_ms - m_seenWindowStart >= (uint64_t) TIME_TO_LIVE_MS)
    // {
        // m_seenPrev.swap (m_seenCurr);
        // m_seenCurr.clear ();
        // m_seenWindowStart = now_ms;
    // }
    // if (m_seenCurr.count (key) || m_seenPrev.count (key)) return true;
    // m_seenCurr.insert (key);
    // return false;
// }

bool
RoutingRingSwitchDelta::SeenBefore (uint16_t origin, uint32_t id)
{
    if (m_seen.size () <= origin) m_seen.resize (origin + 1);
    SeenWindow& w = m_seen[origin];

    if (!w.init) {
        w.bits.assign (SEEN_WORDS, 0);
        w.base = (id >= SEEN_W / 2 ? id - SEEN_W / 2 : 0) & ~63u;
        w.init = true;
    }

    // Older than the window: it is TTL-expired anyway, so treat it as seen.
    if (id < w.base) { 
        m_seen_too_old++; 
        printf("SEEN TOO OLD\n");
        return true; 
    }

    if (id >= w.base + SEEN_W) {                       // slide forward
        uint32_t newBase    = (id - SEEN_W + 1) & ~63u;
        uint32_t shiftWords = (newBase - w.base) / 64;
        if (shiftWords >= SEEN_WORDS) {
            std::fill (w.bits.begin (), w.bits.end (), 0);
        } else {
            std::move (w.bits.begin () + shiftWords, w.bits.end (), w.bits.begin ());
            std::fill (w.bits.end () - shiftWords, w.bits.end (), 0);
        }
        w.base = newBase;
    }

    uint32_t  off  = id - w.base;
    uint64_t  mask = 1ull << (off & 63);
    uint64_t& word = w.bits[off >> 6];
    if (word & mask) return true;
    word |= mask;
    return false;
}


// ─────────────────────────────────────────────────────────────────────────────
// Init
// ─────────────────────────────────────────────────────────────────────────────
void RoutingRingSwitchDelta::Init(SatelliteForwardingApp *app)
{
    m_app = app;

    // <output folder>/packet_stats/packet_monitoring_dataFix.csv
    SatelliteForwardingApp::OutputPath ("packet_stats/packet_monitoring_dataFix.csv",
                                        filename_packet_monitoring, sizeof (filename_packet_monitoring));
    if (m_app->GetSatId() == 0) {
        FILE* f = fopen(filename_packet_monitoring, "w");
        if (f) { fprintf(f, "node,time,packet_id,n,p,cameLeft\n"); fclose(f); }

    }

    // Start the shared shortest-path-tree rebuild loop exactly once. This is
    // deliberately NOT gated on GetSatId()==0: sat ids may not be assigned yet
    // when Init() runs, which would either start no loop at all or start one
    // per satellite. g_loopStarted makes it idempotent either way.
    // if (!g_loopStarted) {
        // g_loopStarted = true;
        // if (kBroadcastMode == BCAST_SPT)
            // Simulator::Schedule (Seconds (kFirstBuildDelay), &SptRebuildLoop);
        // if (kBcastStats)
            // Simulator::Schedule (Seconds (2), &BcastStatsLoop);
    // }
}

// ─────────────────────────────────────────────────────────────────────────────
// Ring roles (overview 2 and 4)
// ─────────────────────────────────────────────────────────────────────────────

// Exit positions at epoch 0 from the constellation config (one ring satellite
// per orbit and ring) and the sat ids of the left / right ISL neighbours.
void RoutingRingSwitchDelta::InitRoles ()
{
    const ConstellationConfig& cfg = m_app->GetTopoConfig ();
    const uint32_t S = m_app->GetSatsPerOrbit ();
    const uint32_t O = m_app->m_numOrbits;
    m_upExit0.assign (O, -1);
    m_downExit0.assign (O, -1);
    for (uint32_t s : cfg.ringUp) {
        if (s / S >= O) continue;
        NS_ABORT_MSG_IF (m_upExit0[s / S] >= 0, "walker-delta: more than one ringUp satellite in orbit " << s / S);
        m_upExit0[s / S] = (int32_t) (s % S);
    }
    for (uint32_t s : cfg.ringDown) {
        if (s / S >= O) continue;
        NS_ABORT_MSG_IF (m_downExit0[s / S] >= 0, "walker-delta: more than one ringDown satellite in orbit " << s / S);
        m_downExit0[s / S] = (int32_t) (s % S);
    }
    Ptr<Node> l = PeerNode (m_app->GetDevLeft ());
    Ptr<Node> r = PeerNode (m_app->GetDevRight ());
    m_leftPeer  = l ? l->GetId () : UINT32_MAX;    // node id == sat id
    m_rightPeer = r ? r->GetId () : UINT32_MAX;
    m_rolesInit = true;
    UpdateUpRole   (m_maxUpEpochSeen,   false, false);
    UpdateDownRole (m_maxDownEpochSeen);
}

bool RoutingRingSwitchDelta::IsUpExitSat (uint32_t sat, uint32_t epoch) const
{
    const uint32_t S = m_app->GetSatsPerOrbit ();
    const uint32_t o = sat / S;
    if (sat == UINT32_MAX || o >= m_upExit0.size () || m_upExit0[o] < 0) return false;
    return sat % S == ((uint32_t) m_upExit0[o] + S - epoch % S) % S;
}

bool RoutingRingSwitchDelta::IsDownExitSat (uint32_t sat, uint32_t epoch) const
{
    const uint32_t S = m_app->GetSatsPerOrbit ();
    const uint32_t o = sat / S;
    if (sat == UINT32_MAX || o >= m_downExit0.size () || m_downExit0[o] < 0) return false;
    return sat % S == ((uint32_t) m_downExit0[o] + S - epoch % S) % S;
}

// Role of this satellite in the UP ring for `epoch`: exit, (kink) entry or
// none. isRingUp marks ring members (exit or entry): their queues get the
// enlarged ring buffer (SatelliteForwardingApp::AdjustQueueBuffer).
void RoutingRingSwitchDelta::UpdateUpRole (uint32_t epoch, bool announce, bool initiator)
{
    const bool wasExit = m_upExit;
    const bool wasRing = m_app->isRingUp;
    const bool entry   = IsUpExitSat (m_leftPeer, epoch);
    m_upExit        = IsUpExitSat (m_app->GetSatId (), epoch);
    m_app->isRingUp = m_upExit || entry;
    // new ring member: ring buffer before the first ring packet is queued
    if (m_app->isRingUp && !wasRing && m_rolesInit) m_app->GrowRingQueuesNow ();
    // exit of the kink orbit: its UP link carries no ring traffic (overview 2)
    m_app->m_noInjectUp = m_upExit && !entry;
    if (!announce) return;
    if (m_upExit && !wasExit) {
        printf("[RingSwitchDelta] t=%.3fs  sat%u: %s ring-UP exit (epoch %u)\n",
               Simulator::Now().GetSeconds(), m_app->GetSatId(),
               initiator ? "INITIATING switch, new" : "activated as new", epoch);
        // new ring ISL gets the queueing delay of the old one (dummies),
        // also on the initiator (overview 5)
        InsertDummyPackets(m_app->GetIslQueueSize(), m_app->GetDevRight());
    } else if (!m_upExit && wasExit) {
        printf("[RingSwitchDelta] t=%.3fs  sat%u: ring-UP exit retired (epoch %u)\n",
               Simulator::Now().GetSeconds(), m_app->GetSatId(), epoch);
    }
}

// DOWN ring membership for `epoch` (exit or kink entry, enlarged queues) and
// the kink-orbit injection block. DOWN copies are routed by their own epoch
// (RouteDown), so a role change has no routing effect here.
void RoutingRingSwitchDelta::UpdateDownRole (uint32_t epoch)
{
    const bool wasRing = m_app->isRingDown;
    const bool entry   = IsDownExitSat (m_rightPeer, epoch);
    m_downExit        = IsDownExitSat (m_app->GetSatId (), epoch);
    m_app->isRingDown = m_downExit || entry;
    // new ring member (e.g. the new entry, which may get the old and the new
    // stream at the same time for a moment): ring buffer before the first
    // ring packet is queued
    if (m_app->isRingDown && !wasRing && m_rolesInit) m_app->GrowRingQueuesNow ();
    // exit of the kink orbit: its DOWN link carries no ring traffic (overview 2)
    m_app->m_noInjectDown = m_downExit && !entry;
}

// Dummy Packet Helper
void RoutingRingSwitchDelta::InsertDummyPackets(uint32_t amount, Ptr<NetDevice> device)
{
    
    Ptr<Queue<Packet>> q = DynamicCast<PointToPointLaserNetDevice>(device)->GetQueue();
    uint64_t qSize = q->GetCurrentSize().GetValue();
    while (amount > qSize+1)
    {
        Ptr<Packet> pkt_new = Create<Packet>(SatelliteForwardingApp::maxPayloadSize);
        RingSwitchDeltaHeader hdr_new;
        hdr_new.SetId       (0);
        hdr_new.SetSat      (0);
        hdr_new.SetTTL      (0);
        hdr_new.SetDirection(DUMMY);
        hdr_new.SetEpoch    (0);
        hdr_new.SetObjId    (0);
        hdr_new.SetFragId   (0);
        hdr_new.SetLastHop  (0);
        hdr_new.SetDupCode  (0);
        pkt_new->AddHeader(hdr_new);
        device->Send(pkt_new, device->GetBroadcast(), SatelliteForwardingApp::PROTO);
        amount--;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Ring switch initiation (overview 5). Called by the RingSwitchScheduler (via
// SatelliteForwardingApp::TriggerRing*) on the new exit of the kink orbit.
// ─────────────────────────────────────────────────────────────────────────────
void RoutingRingSwitchDelta::InitiateRingUpSwitch()
{
    if (!m_rolesInit) InitRoles ();
    m_maxUpEpochSeen = m_maxUpEpochSeen + 1;
    UpdateUpRole (m_maxUpEpochSeen, true, true);
    if (!m_upExit) {
        printf("[RingSwitchDelta] t=%.3fs  sat%u: WARNING ring-UP initiator is not the exit of epoch %u (check ringUp in the constellation config)\n",
               Simulator::Now().GetSeconds(), m_app->GetSatId(), m_maxUpEpochSeen);
    }
}

void RoutingRingSwitchDelta::InitiateRingDownSwitch()
{
    if (!m_rolesInit) InitRoles ();
    m_downInitiated    = m_maxDownEpochSeen + 1;
    m_maxDownEpochSeen = m_downInitiated;
    UpdateDownRole (m_maxDownEpochSeen);
    printf("[RingSwitchDelta] t=%.3fs  sat%u: INITIATING ring-DOWN switch (epoch %u)\n",
           Simulator::Now().GetSeconds(), m_app->GetSatId(), m_downInitiated);
    // must be the new exit of the DOWN kink orbit (= its old entry)
    if (!(m_downExit && !IsDownExitSat (m_rightPeer, m_downInitiated))) {
        printf("[RingSwitchDelta] t=%.3fs  sat%u: WARNING ring-DOWN initiator is not the exit of the kink orbit for epoch %u (check ringDown / seamRightDown in the constellation config)\n",
               Simulator::Now().GetSeconds(), m_app->GetSatId(), m_downInitiated);
    }
}


// ─────────────────────────────────────────────────────────────────────────────
// Duplication logic (content-fill-double) 
// ─────────────────────────────────────────────────────────────────────────────
bool RoutingRingSwitchDelta::DuplicatePacket(direction_t direction, uint32_t id, uint16_t origin, 
                                        uint64_t ttl, uint32_t obj_id, uint32_t frag_id, uint64_t time)
{

    Ptr<NetDevice> duplication_dev = nullptr;
    uint32_t epoch = 0;
    if      (direction == DOWN) {
        duplication_dev = m_app->GetDevDown();
        epoch = m_maxDownEpochSeen;
    }
    else if (direction == UP) {
        duplication_dev = m_app->GetDevUp();
        epoch = m_maxUpEpochSeen;
    }

    bool can_duplicate = true;
    // kink-orbit exit: no copies into its dead-end orbit link (overview 2)
    if ((direction == UP && m_app->m_noInjectUp) || (direction == DOWN && m_app->m_noInjectDown)) return false;
    // Periodic storage: second copies only during the fill phase (as for
    // Walker-Star), otherwise they add packets after the budget is frozen.
    if (Simulator::Now().GetSeconds() >= STORAGE_FILL_PHASE_END_S) return false;
    // Don't do this during ring switches
    for (int i=0; i<m_app->switch_times.size(); i++){
        if (Simulator::Now().GetSeconds() >= m_app->switch_times[i] - 1
            && Simulator::Now().GetSeconds() <= m_app->switch_times[i] + 3)
            { // we are within a switch period (safety margin = 3 seconds)
                can_duplicate = false;
            }
    }
    if (duplication_dev != nullptr && can_duplicate) {
        Ptr<Queue<Packet>> q = DynamicCast<PointToPointLaserNetDevice>(duplication_dev)->GetQueue();
        uint64_t qSize = q->GetCurrentSize().GetValue();
        if (qSize < m_app->GetIslQueueSize()) {
            Ptr<Packet> pkt_new = Create<Packet>(SatelliteForwardingApp::maxPayloadSize);
            RingSwitchDeltaHeader hdr_new;
            hdr_new.SetId       (id);
            hdr_new.SetSat      (origin);
            hdr_new.SetTime     (time);
            hdr_new.SetTTL      (ttl);
            hdr_new.SetDirection(direction);
            hdr_new.SetEpoch    (epoch);
            hdr_new.SetObjId    (obj_id);
            hdr_new.SetFragId   (frag_id);
            hdr_new.SetLastHop  (m_app->GetSatId());
            hdr_new.SetDupCode  (0);
            pkt_new->AddHeader(hdr_new);
            duplication_dev->Send(pkt_new, duplication_dev->GetBroadcast(), SatelliteForwardingApp::PROTO);

            { FILE* f = fopen(m_app->filename_obj_dup, "a");
              if (f) { fprintf(f, "%lf,%u,%u,%u,%u\n", Simulator::Now().GetSeconds(),
                               m_app->GetSatId(), (uint32_t)origin,
                               obj_id , frag_id); fclose(f); } }

            if (direction == UP) m_app->m_bytesSentUp++;
            if (direction == DOWN) m_app->m_bytesSentDown++;
            return true;
        }
    }
    return false;
}


// ─────────────────────────────────────────────────────────────────────────────
// Routing tables 
// ─────────────────────────────────────────────────────────────────────────────
// A copy that arrived on a port without rule is not forwarded, i.e. lost:
// count it as dropped and log it (code 3: UP copy, code 0: DOWN copy).
void RoutingRingSwitchDelta::LogUnknownDevice(int code, uint32_t last_hop)
{
    m_app->num_dropped_packets++;
    FILE* f = fopen(filename_packet_monitoring, "a");
    if (f) {
        fprintf(f, "%u,%u,%lf,%d\n", m_app->GetSatId(), last_hop, Simulator::Now().GetSeconds(), code);
        fclose(f);
    }
}

// UP copies (overview 3)
Ptr<NetDevice> RoutingRingSwitchDelta::RouteUp(Ptr<NetDevice> device, bool& crossesRing) const
{
    crossesRing = false;
    if (device == m_app->GetDevLeft())  return m_app->GetDevUp();         // enter the orbit
    if (device == m_app->GetDevDown()) {
        if (m_upExit) { crossesRing = true; return m_app->GetDevRight(); } // lap done -> next orbit
        return m_app->GetDevUp();                                         // pass on
    }
    if (device == m_app->GetDevUp())    return m_app->GetDevDown();       // reverse direction (unused)
    return nullptr;                                                        // devRight: no rule
}

// DOWN queue regulation (overview 6)
void RoutingRingSwitchDelta::RegulateDownQueue ()
{
    Ptr<Queue<Packet>> q = DynamicCast<PointToPointLaserNetDevice>(m_app->GetDevDown())->GetQueue();
    if (!m_downBaseRecorded) {
        if (Simulator::Now().GetSeconds() < STORAGE_FILL_PHASE_END_S) return;
        // steady DOWN level = occupancy at the first DOWN copy after the fill phase
        m_downBaseOcc      = q->GetCurrentSize().GetValue();
        m_downBaseRecorded = true;
    }
    const uint64_t level = std::min<uint64_t>(m_downBaseOcc, m_app->GetIslQueueSize());
    if (q->GetCurrentSize().GetValue() < level)
        InsertDummyPackets((uint32_t) (level + 1), m_app->GetDevDown());   // fills up to level
}

// DOWN copies (overview 3 and 5): routed by the copy's own epoch
Ptr<NetDevice> RoutingRingSwitchDelta::RouteDown(Ptr<NetDevice> device, uint32_t& epoch, bool& crossesRing)
{
    crossesRing = false;
    if (device == m_app->GetDevRight()) {                                  // enter the orbit
        if (epoch < m_downInitiated) epoch = m_downInitiated;              // switch initiator: new epoch
        return m_app->GetDevDown();
    }
    if (device == m_app->GetDevUp()) {
        if (IsDownExitSat (m_app->GetSatId (), epoch)) {                   // lap done -> next orbit
            if (epoch > m_downExitEpoch) {
                // first copy of a new epoch: the new ring ISL gets the
                // queueing delay of the old one (dummies)
                m_downExitEpoch = epoch;
                printf("[RingSwitchDelta] t=%.3fs  sat%u: first ring-DOWN exit of epoch %u\n",
                       Simulator::Now().GetSeconds(), m_app->GetSatId(), epoch);
                InsertDummyPackets(m_app->GetIslQueueSize(), m_app->GetDevLeft());
            }
            crossesRing = true;
            return m_app->GetDevLeft();
        }
        return m_app->GetDevDown();                                       // pass on
    }
    if (device == m_app->GetDevDown())  return m_app->GetDevUp();         // reverse direction (unused)
    return nullptr;                                                        // devLeft: no rule
}

// ─────────────────────────────────────────────────────────────────────────────
// OnReceive
// ─────────────────────────────────────────────────────────────────────────────
bool RoutingRingSwitchDelta::OnReceive(Ptr<NetDevice>    device,
                                  Ptr<const Packet> packet,
                                  uint16_t          protocol,
                                  const Address&    sender)
{
    if (protocol != SatelliteForwardingApp::PROTO &&
        protocol != SatelliteForwardingApp::PROTO_BROADCAST)
            return false;

    Ptr<Packet> pkt = packet->Copy();
    RingSwitchDeltaHeader hdr;
    pkt->RemoveHeader(hdr);

    uint64_t id             = hdr.GetId();
    uint32_t origin         = hdr.GetSat();
    uint64_t time           = hdr.GetTime();
    uint32_t epoch          = hdr.GetEpoch();
    uint32_t last_hop       = hdr.GetLastHop();
    uint16_t dup_code       = hdr.GetDupCode();
    uint64_t ttl            = hdr.GetTTL();
    uint32_t obj_id         = hdr.GetObjId();
    uint32_t frag_id        = hdr.GetFragId();
    uint16_t sat_id         = m_app->GetSatId();
    uint64_t now_ms         = Simulator::Now ().GetMilliSeconds ();
    direction_t direction = (direction_t)hdr.GetDirection();

    // Dummy packets are just for timing, drop them silently
    if (direction == DUMMY){
        return true;
    }

    if (now_ms - time > ttl*1000){
        // TODO: delete them from the entries
        return true;
    }

    // ── Broadcast logging / stats ───────────────────────────────
    if      (device == m_app->GetDevDown())  m_app->m_bytesRecvDown++;
    else if (device == m_app->GetDevUp())    m_app->m_bytesRecvUp++;
    else if (device == m_app->GetDevLeft())  m_app->m_bytesRecvLeft++;
    else if (device == m_app->GetDevRight()) m_app->m_bytesRecvRight++;

#if defined(MONITORE_PACKET) && (MONITORE_PACKET == 1)
    FILE* f = fopen(filename_packet_monitoring, "a");
    if (f) {
        fprintf(f, "%u,%lf,%lu,%u\n", sat_id, Simulator::Now().GetSeconds(), id+hdr.GetDirection(), epoch);
        fclose(f);
    }
#endif

    double curr_time = Simulator::Now().GetSeconds();
    if (origin == sat_id && direction != BROADCAST && direction != BROADCAST_PRUNE) {
        SatelliteForwardingApp::PacketRecord* rec = m_app->GetRecord ((uint32_t) id);
        if (rec)
        {
            if (direction == UP || direction == DOWN)
            {
                m_app->RecordReturn (rec, direction == UP, curr_time);   // RTT of this copy
            }
            if (dup_code != 0) {
                FILE* f = fopen(m_app->filename_reassemble, "a");
                if (f) {
                    fprintf(f, "%u,%lf,%u,%u,%lu\n", sat_id, Simulator::Now().GetSeconds(), dup_code, origin, id);
                    fclose(f);
                }
            }
            rec->lastSeen = curr_time;
        }
    }
    // }
    // Difference Doppler effect reality, simulation

    if ((direction == DOWN || direction == UP) && !m_rolesInit) InitRoles ();

    if (direction == DOWN){
        // Route first: the switch initiator gives copies entering there the
        // new epoch (overview 5).
        bool crossesRing = false;
        Ptr<NetDevice> out = RouteDown (device, epoch, crossesRing);

        // Newest DOWN epoch seen (ring membership, next switch initiation)
        if (epoch > m_maxDownEpochSeen) {
            m_maxDownEpochSeen = epoch;
            UpdateDownRole (epoch);
        }
        // Epoch of the last DOWN packet seen (content strategy: epoch of new
        // packets, FREEZE_AFTER_FIRST_DOWN_SWITCH).
        m_app->m_currDownEpoch = epoch;

        if (dup_code > 0){
            if (DuplicatePacket(UP, id, origin, ttl, obj_id, frag_id, time)){
                hdr.SetDupCode(0);
            }
        }

        if (!out) { LogUnknownDevice (0, last_hop); return true; }
        if (out == m_app->GetDevDown()) RegulateDownQueue ();
        hdr.SetEpoch (epoch);
        hdr.SetLastHop(sat_id);
        pkt->AddHeader(hdr);
        m_app->ForwardPacket (out, pkt, true);

    } else if (direction == UP){
        if (epoch > m_maxUpEpochSeen) {
            m_maxUpEpochSeen = epoch;
            UpdateUpRole (epoch, true, false);
        }
        m_app->m_currUpEpoch = m_maxUpEpochSeen;

        if (dup_code > 0){
            if(DuplicatePacket(DOWN, id, origin, ttl, obj_id, frag_id, time)){
                hdr.SetDupCode(0);
            }
        }

        bool crossesRing = false;
        Ptr<NetDevice> out = RouteUp (device, crossesRing);
        if (!out) { LogUnknownDevice (3, last_hop); return true; }
        if (crossesRing) hdr.SetEpoch (m_maxUpEpochSeen);
        hdr.SetLastHop(sat_id);
        pkt->AddHeader(hdr);
        m_app->ForwardPacket (out, pkt, true);

    } else if (direction == BROADCAST_PRUNE){
        // Control packet: the neighbour on this link told us it already has
        // source `origin` by a faster path, so stop sending it that way.
        // Deliberately handled before the origin==sat_id guard, since a
        // satellite can legitimately be pruned for its own traffic.
        ApplyPrune (m_app, origin, device);
        return true;

    } else if (direction == BROADCAST && protocol == SatelliteForwardingApp::PROTO_BROADCAST){

        if (origin == sat_id) return true;

        if (SeenBefore (origin, id)) {
            ++g_dupRecv;
            if (kBroadcastMode == BCAST_PRUNE) {
                // The whole algorithm, right here: a duplicate proves this link
                // is not on our minimum-delay path from `origin`, so prune it.
                SendPrune (m_app, origin, device);
            } else if (kBroadcastMode != BCAST_FLOOD) {
                // For the two precomputed trees this must NEVER fire: a correct
                // tree delivers every packet to every satellite exactly once.
                // Treat a rising count as an assertion failure.
                g_sptDuplicates++;
            }
            return true;
        }

        ++g_firstRecv;
        // The link this first copy arrived on is, by definition, the
        // minimum-delay path from `origin`. No clock, no probe, no measurement.
        if (kBroadcastMode == BCAST_PRUNE) RecordParent (m_app, origin, last_hop);

        if (dup_code > 0) { // Log the time the satellite received the packet only
                            // for some packets such that we do not slow down the simulation too much
            FILE* f = fopen(m_app->filename_broadcast_stats, "a");
            if (f) {
                fprintf(f, "0,%u,%u,%lu,%lf\n", sat_id, origin, id, Simulator::Now().GetSeconds());
                fclose(f);
            }
        } 

        // ── 1. Epidemic flooding ────────────────────────────────────────────
        if (kBroadcastMode == BCAST_FLOOD) {

            hdr.SetLastHop(sat_id);
            pkt->AddHeader(hdr);

            std::vector<Ptr<NetDevice>> devices = {
                m_app->GetDevUp(),
                m_app->GetDevDown(),
                m_app->GetDevLeft(),
                m_app->GetDevRight(),
            };
            for (auto dev : devices){
                if (dev == device || !dev) continue;
                ++g_txCopies;
                m_app->ForwardPacket(dev, pkt->Copy(), true, SatelliteForwardingApp::PROTO_BROADCAST);
            }
        }

        // ── 4. Distributed reverse-path pruning ──────────────────────────────
        //      Same forwarding rule as flooding, minus the links a neighbour
        //      has pruned. Converges to the delay-optimal tree after one flood
        //      per source, with no topology knowledge at all.
        else if (kBroadcastMode == BCAST_PRUNE) {

            hdr.SetLastHop(sat_id);
            pkt->AddHeader(hdr);

            std::vector<Ptr<NetDevice>> devices = {
                m_app->GetDevUp(),
                m_app->GetDevDown(),
                m_app->GetDevLeft(),
                m_app->GetDevRight(),
            };
            for (auto dev : devices){
                if (dev == device || !dev) continue;
                if (IsPruned(m_app, origin, dev)) continue;
                ++g_txCopies;
                m_app->ForwardPacket(dev, pkt->Copy(), true, SatelliteForwardingApp::PROTO_BROADCAST);
            }
        }

        // ── 2. Dimension-ordered (YX) spanning tree ──────────────────────────
        //      last_hop is reused as the per-phase hop counter. Note the
        //      asymmetric limits: with an even ring size, running N/2 in both
        //      directions makes the two chains collide at the antipodal node.
        else if (kBroadcastMode == BCAST_DIMORDER) {

            const uint32_t maxUp    = m_app->GetSatsPerOrbit() / 2;
            const uint32_t maxDown  = (m_app->GetSatsPerOrbit() - 1) / 2;
            const uint32_t maxRight = m_app->m_numOrbits / 2;
            const uint32_t maxLeft  = (m_app->m_numOrbits - 1) / 2;

            auto emit = [&](Ptr<NetDevice> dev, uint32_t hops) {
                if (!dev) return;
                auto h = hdr;                 // set the counter BEFORE serialising
                h.SetLastHop(hops);
                Ptr<Packet> copy = pkt->Copy();
                copy->AddHeader(h);
                ++g_txCopies;
                m_app->ForwardPacket(dev, copy, true,
                                     SatelliteForwardingApp::PROTO_BROADCAST);
            };

            if (device == m_app->GetDevDown()) {            // travelling up, intra-plane
                if (last_hop < maxUp)    emit(m_app->GetDevUp(),    last_hop + 1);
            }
            else if (device == m_app->GetDevUp()) {         // travelling down, intra-plane
                if (last_hop < maxDown)  emit(m_app->GetDevDown(),  last_hop + 1);
            }
            else if (device == m_app->GetDevLeft()) {       // travelling right, cross-plane
                if (last_hop < maxRight) emit(m_app->GetDevRight(), last_hop + 1);
                emit(m_app->GetDevUp(),   1);               // spawn: UNCONDITIONAL,
                emit(m_app->GetDevDown(), 1);               // the last plane needs covering too
            }
            else if (device == m_app->GetDevRight()) {      // travelling left, cross-plane
                if (last_hop < maxLeft)  emit(m_app->GetDevLeft(),  last_hop + 1);
                emit(m_app->GetDevUp(),   1);
                emit(m_app->GetDevDown(), 1);
            }
        }

        // ── 3. Delay-weighted shortest-path tree ─────────────────────────────
        //      No counters, no geometric tests: the precomputed tree already
        //      encodes all of it. Re-testing here would prune valid tree links.
        else {

            hdr.SetLastHop(sat_id);
            pkt->AddHeader(hdr);

            SptForward(m_app, origin, pkt, device);
        }
    }
    
    return true;
}

} // namespace ns3