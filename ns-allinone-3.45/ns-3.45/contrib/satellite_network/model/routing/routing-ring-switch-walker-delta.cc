// Storage ring routing for Walker-Delta constellations.
//
// Topology
//   devUp / devDown connect a satellite to the next / previous satellite of its
//   orbit, devRight / devLeft to the satellite of the next / previous orbit
//   (sat id = orbit * satellitesPerOrbit + position). The cross-plane ISLs
//   connect equal positions, except between the last and the first orbit,
//   where the Walker phasing shifts them by one position. There is no seam:
//   both rings are closed.
//
// Rings (RouteUp / RouteDown)
//   Every object is stored as an UP copy, which travels upwards in its orbit,
//   and a DOWN copy, which travels downwards. A copy goes once around its
//   orbit and leaves it at the exit satellite of the orbit (UP: to the right,
//   DOWN: to the left). It enters the next orbit at that orbit's entry, the
//   ISL neighbour of the previous exit. All exits lie at the same position
//   (ringUp / ringDown of the constellation config) and every entry equals the
//   exit of its orbit, except in one orbit per ring (kink orbit), where the
//   phase shift puts the entry one position next to the exit. The link from
//   the kink exit along its orbit carries no ring traffic, so the kink exit
//   inserts no copies in that direction (SetInsertionBlocked).
//
// Epochs and ring switches (UpdateUpRole / UpdateDownRole / Initiate*)
//   The epoch is the number of ring switches so far; at every switch all exits
//   move down by one position (IsUpExit / IsDownExit). The scheduler starts a
//   switch on the new exit of the kink orbit.
//   UP: a satellite takes its role of epoch e when it first sees a copy with
//   epoch e; copies that leave an orbit carry the epoch of the exit. The new
//   exit lies one hop before the old one, so copies in flight make one hop
//   less in one orbit, once.
//   DOWN: the new exit lies one hop after the old one, so a role change would
//   send copies that have just entered an orbit out of it again. Instead every
//   DOWN copy leaves its orbit at the exit of the epoch it carries. The
//   initiator gives the new epoch to the copies entering there, and the new
//   epoch spreads with the copies, one lap per orbit.
//   Copies that still arrive at a retired satellite on a ring port enter the
//   orbit; they are never lost.
//
// Dummy packets
//   A new exit fills its ring ISL queue with dummies before it starts exiting
//   (UP: role change, DOWN: first copy of a new epoch), so that the new ring
//   ISL has the queueing delay of the old one. Every satellite tops its
//   storage queues up to their levels before it enqueues a copy
//   (SatelliteForwardingApp::RegulateStorageQueue): the old entries of a DOWN
//   switch receive no copies for one lap and would drain otherwise.
//
// Stored copies (ReceiveUp / ReceiveDown)
//   A copy that was inserted alone carries a flag; satellites that forward it
//   create the missing copy in the other direction while their queue is still
//   filling (SatelliteForwardingApp::CreateSecondCopy). A copy whose time to
//   live has expired is replaced or deleted by its origin, when it passes
//   there (SatelliteForwardingApp::ReplaceExpiredOwnCopy).
//
// Broadcasts (ReceiveBroadcast, DeltaBroadcastAlgorithm)
//   The routing drops duplicates and hands every broadcast to the broadcast
//   algorithm selected with --broadcastAlgorithm, which also chooses the first
//   hops of the broadcasts of its own satellite. Algorithms (kBroadcastAlgorithms):
//     prune      flooding with reverse-path pruning (PruneBroadcast, default)
//     flood      flooding (FloodBroadcast)
//     dim-order  dimension-ordered spanning tree (DimensionOrderBroadcast)
//     spt        delay-optimal shortest-path trees (ShortestPathTreeBroadcast)

#include "routing-ring-switch-walker-delta.h"

#include "ns3/channel.h"
#include "ns3/mobility-model.h"
#include "ns3/mpi-interface.h"
#include "ns3/node-list.h"
#include "ns3/node.h"
#include "ns3/point-to-point-laser-net-device.h"
#include "ns3/simulator.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <limits>
#include <queue>
#include <utility>

namespace ns3
{

namespace
{

/// Codes in the log of unroutable packets.
constexpr int kUnroutableDown = 0;
constexpr int kUnroutableUp = 3;

/// Node on the other end of the ISL of `device` (node id = sat id).
Ptr<Node>
PeerNode(Ptr<NetDevice> device)
{
    if (!device)
    {
        return nullptr;
    }
    if (Ptr<PointToPointLaserNetDevice> laser = DynamicCast<PointToPointLaserNetDevice>(device))
    {
        if (Ptr<Node> node = laser->GetDestinationNode())
        {
            return node;
        }
    }
    Ptr<Channel> channel = device->GetChannel();
    if (!channel || channel->GetNDevices() != 2)
    {
        return nullptr;
    }
    for (std::size_t i = 0; i < channel->GetNDevices(); i++)
    {
        if (channel->GetDevice(i) != device)
        {
            return channel->GetDevice(i)->GetNode();
        }
    }
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// Broadcast algorithms
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Flooding: every satellite forwards a broadcast to all neighbours except the
 * one it came from.
 */
class FloodBroadcast : public DeltaBroadcastAlgorithm
{
  public:
    void Forward(Ptr<NetDevice> device, Ptr<Packet> packet, SatPacketHeader header) override
    {
        header.SetLastHop(m_app->GetSatId());
        for (Ptr<NetDevice> out : GetDevices())
        {
            if (out != device)
            {
                Send(out, packet, header);
            }
        }
    }
};

/**
 * Flooding with reverse-path pruning: a satellite that receives a duplicate on
 * a port asks the neighbour behind that port to stop forwarding the broadcasts
 * of this origin there (prune message, soft state that expires). After one
 * flood per origin the broadcasts follow the tree of the first arrivals, the
 * delay-optimal tree, without any topology knowledge.
 */
class PruneBroadcast : public DeltaBroadcastAlgorithm
{
  public:
    void Forward(Ptr<NetDevice> device, Ptr<Packet> packet, SatPacketHeader header) override
    {
        const uint16_t origin = header.GetOrigin();
        header.SetLastHop(m_app->GetSatId());
        const std::array<Ptr<NetDevice>, 4> devices = GetDevices();
        for (int port = 0; port < 4; port++)
        {
            if (devices[port] != device && !IsPruned(origin, port))
            {
                Send(devices[port], packet, header);
            }
        }
    }

    void OnDuplicate(Ptr<NetDevice> device, const SatPacketHeader& header) override
    {
        // `device` is not on the fastest path from the origin: prune it
        const int port = GetPortIndex(device);
        if (port < 0)
        {
            return;
        }
        const double now = Simulator::Now().GetSeconds();
        double& lastSent = GetState(header.GetOrigin()).lastPruneSent[port];
        if (now < lastSent + kRefresh)
        {
            return;
        }
        lastSent = now;

        SatPacketHeader prune;
        prune.SetOrigin(header.GetOrigin());
        prune.SetCreationTimeMs(Simulator::Now().GetMilliSeconds());
        prune.SetTtl(kTtl);
        prune.SetDirection(BROADCAST_CONTROL);
        prune.SetLastHop(m_app->GetSatId());
        Send(device, Create<Packet>(0), prune);
    }

    void OnControl(Ptr<NetDevice> device, const SatPacketHeader& header) override
    {
        // the neighbour behind `device` receives the broadcasts of this origin faster on another path
        const int port = GetPortIndex(device);
        if (port >= 0)
        {
            GetState(header.GetOrigin()).prunedUntil[port] = Simulator::Now().GetSeconds() + kLifetime;
        }
    }

  private:
    /// A prune stops the forwarding of one origin on one port for this long [s].
    static constexpr double kLifetime = 30.0;
    /// At most one prune per origin and port in this interval [s].
    static constexpr double kRefresh = 10.0;
    /// Time to live of prune messages [s].
    static constexpr uint64_t kTtl = 10;

    /// Pruning state of one origin, per port.
    struct State
    {
        std::array<double, 4> prunedUntil{0.0, 0.0, 0.0, 0.0};      ///< no forwarding until then [s]
        std::array<double, 4> lastPruneSent{-1e9, -1e9, -1e9, -1e9}; ///< [s]
    };

    State& GetState(uint16_t origin)
    {
        if (m_states.size() <= origin)
        {
            m_states.resize(origin + 1);
        }
        return m_states[origin];
    }

    bool IsPruned(uint16_t origin, int port) const
    {
        return origin < m_states.size() && Simulator::Now().GetSeconds() < m_states[origin].prunedUntil[port];
    }

    std::vector<State> m_states; ///< indexed by origin
};

/**
 * Dimension-ordered spanning tree: the broadcast travels from the origin along
 * its row across the orbits (left and right), and every satellite it reaches
 * starts a chain up and down its orbit. The chains are as long as needed to
 * reach every satellite exactly once; the hop count of the current chain is
 * carried in the last-hop field.
 */
class DimensionOrderBroadcast : public DeltaBroadcastAlgorithm
{
  public:
    void Init(SatelliteForwardingApp* app) override
    {
        DeltaBroadcastAlgorithm::Init(app);
        const std::array<Ptr<NetDevice>, 4> devices = GetDevices();
        for (int port = 0; port < 4; port++)
        {
            Ptr<Node> peer = PeerNode(devices[port]);
            m_peers[port] = peer ? peer->GetId() : UINT32_MAX;
        }
    }

    void Forward(Ptr<NetDevice> device, Ptr<Packet> packet, SatPacketHeader header) override
    {
        const int port = GetPortIndex(device);
        if (port < 0)
        {
            return;
        }
        // hops of the current chain so far, including the one to this satellite
        const uint32_t hops = (m_peers[port] == header.GetOrigin()) ? 1 : header.GetLastHop();
        const uint32_t satsPerOrbit = m_app->GetSatsPerOrbit();
        const uint32_t numOrbits = m_app->GetNumOrbits();
        auto emit = [&](Ptr<NetDevice> out, uint32_t chainHops) {
            SatPacketHeader copy = header;
            copy.SetLastHop(chainHops);
            Send(out, packet, copy);
        };

        if (device == m_app->GetDevDown()) // travelling up the orbit
        {
            if (hops < satsPerOrbit / 2)
            {
                emit(m_app->GetDevUp(), hops + 1);
            }
        }
        else if (device == m_app->GetDevUp()) // travelling down the orbit
        {
            if (hops < (satsPerOrbit - 1) / 2)
            {
                emit(m_app->GetDevDown(), hops + 1);
            }
        }
        else // travelling along the row: continue it and start the orbit chains
        {
            const bool right = (device == m_app->GetDevLeft());
            if (hops < (right ? numOrbits / 2 : (numOrbits - 1) / 2))
            {
                emit(right ? m_app->GetDevRight() : m_app->GetDevLeft(), hops + 1);
            }
            emit(m_app->GetDevUp(), 1);
            emit(m_app->GetDevDown(), 1);
        }
    }

  private:
    std::array<uint32_t, 4> m_peers{UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX}; ///< sat id per port
};

/**
 * Delay-optimal broadcast trees of all origins: shared by all satellites of
 * this process and rebuilt periodically from the satellite positions (link
 * delay = length / speed of light). The trees are built from the nodes, not
 * from the applications, so that every MPI rank builds the same trees.
 * Requires that all nodes are satellites (node id = sat id).
 */
class ShortestPathTrees
{
  public:
    static ShortestPathTrees& Get()
    {
        static ShortestPathTrees trees;
        return trees;
    }

    /// Starts the periodic rebuild (only the first call has an effect).
    void Start(uint32_t satsPerOrbit, uint32_t numOrbits)
    {
        if (m_started)
        {
            return;
        }
        m_started = true;
        m_satsPerOrbit = satsPerOrbit;
        m_numOrbits = numOrbits;
        Simulator::Schedule(Seconds(kFirstBuild), &ShortestPathTrees::RebuildLoop, this);
    }

    /// Devices on which `sat` forwards the broadcasts of `origin` (its children
    /// in the tree of `origin`). False while no valid tree exists.
    bool GetChildren(uint32_t sat, uint32_t origin, std::vector<Ptr<NetDevice>>& children) const
    {
        children.clear();
        if (!m_valid || sat >= m_children.size() || origin >= m_children.size())
        {
            return false;
        }
        for (int slot = 0; slot < 4; slot++)
        {
            if ((m_children[sat][origin] >> slot) & 1U)
            {
                children.push_back(m_devices[sat][slot]);
            }
        }
        return true;
    }

    /// A satellite received a broadcast twice (never happens with valid trees).
    void CountDuplicate() { m_numDuplicates++; }

  private:
    static constexpr double kSpeedOfLight = 299792458.0; ///< [m/s]
    static constexpr double kFirstBuild = 0.1;           ///< [s]
    static constexpr double kRebuildInterval = 10.0;     ///< [s]
    static constexpr double kRetryInterval = 0.5;        ///< while the topology is incomplete [s]
    static constexpr uint32_t kMaxReportedFailures = 200;
    static constexpr double kInfinity = std::numeric_limits<double>::infinity();

    void RebuildLoop()
    {
        Rebuild();
        Simulator::Schedule(Seconds(m_valid ? kRebuildInterval : kRetryInterval),
                            &ShortestPathTrees::RebuildLoop,
                            this);
    }

    void Rebuild();
    /// Appends the tree of `origin` to packet_stats/spt_tree.csv.
    void WriteTree(uint32_t origin);
    /// UP / DOWN / LEFT / RIGHT from satellite `from` to its neighbour `to` ("?" if not a grid neighbour).
    const char* GetDirectionName(uint32_t from, uint32_t to) const;

    bool m_started{false};
    bool m_valid{false};
    bool m_csvCreated{false};
    uint32_t m_satsPerOrbit{0};
    uint32_t m_numOrbits{0};
    uint32_t m_numFailures{0};
    uint64_t m_numDuplicates{0};
    std::vector<std::array<Ptr<NetDevice>, 4>> m_devices; ///< [node][slot] laser devices
    std::vector<std::array<int32_t, 4>> m_neighbours;     ///< [node][slot] neighbour node, -1: none
    std::vector<std::array<double, 4>> m_delays;          ///< [node][slot] link delay [s]
    std::vector<std::vector<double>> m_distances;         ///< [origin][node] delay from the origin [s]
    std::vector<std::vector<uint8_t>> m_children;         ///< [node][origin] slots of the children
};

void
ShortestPathTrees::Rebuild()
{
    const uint32_t n = NodeList::GetNNodes();
    m_devices.assign(n, {nullptr, nullptr, nullptr, nullptr});
    m_neighbours.assign(n, {-1, -1, -1, -1});
    m_delays.assign(n, {0.0, 0.0, 0.0, 0.0});

    // links and their delays
    uint32_t numFailures = 0;
    for (uint32_t v = 0; v < n; v++)
    {
        Ptr<Node> node = NodeList::GetNode(v);
        int slot = 0;
        for (uint32_t i = 0; i < node->GetNDevices() && slot < 4; i++)
        {
            if (DynamicCast<PointToPointLaserNetDevice>(node->GetDevice(i)))
            {
                m_devices[v][slot++] = node->GetDevice(i);
            }
        }
        Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
        for (slot = 0; slot < 4; slot++)
        {
            Ptr<Node> peer = PeerNode(m_devices[v][slot]);
            if (!m_devices[v][slot])
            {
                continue;
            }
            if (!peer || peer->GetId() >= n || !mobility || !peer->GetObject<MobilityModel>())
            {
                numFailures++;
                continue;
            }
            m_neighbours[v][slot] = static_cast<int32_t>(peer->GetId());
            m_delays[v][slot] = mobility->GetDistanceFrom(peer->GetObject<MobilityModel>()) / kSpeedOfLight;
        }
    }
    // only links that both ends know
    uint32_t numLinks = 0;
    for (uint32_t v = 0; v < n; v++)
    {
        for (int32_t& u : m_neighbours[v])
        {
            if (u >= 0 && std::find(m_neighbours[u].begin(), m_neighbours[u].end(), static_cast<int32_t>(v)) ==
                              m_neighbours[u].end())
            {
                u = -1;
            }
            numLinks += (u >= 0) ? 1 : 0;
        }
    }

    // Dijkstra from every origin
    m_distances.assign(n, std::vector<double>(n, kInfinity));
    using Entry = std::pair<double, uint32_t>;
    for (uint32_t s = 0; s < n; s++)
    {
        std::vector<double>& dist = m_distances[s];
        std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> queue;
        dist[s] = 0.0;
        queue.emplace(0.0, s);
        while (!queue.empty())
        {
            const auto [d, u] = queue.top();
            queue.pop();
            if (d > dist[u])
            {
                continue;
            }
            for (int slot = 0; slot < 4; slot++)
            {
                const int32_t v = m_neighbours[u][slot];
                if (v >= 0 && d + m_delays[u][slot] < dist[v] - 1e-15)
                {
                    dist[v] = d + m_delays[u][slot];
                    queue.emplace(dist[v], static_cast<uint32_t>(v));
                }
            }
        }
    }

    // parent of every node in every tree: the origin itself if it is a
    // neighbour on a shortest path, otherwise the cheapest neighbour (ties:
    // lowest sat id, so that every rank builds the same trees)
    m_children.assign(n, std::vector<uint8_t>(n, 0));
    for (uint32_t s = 0; s < n; s++)
    {
        const std::vector<double>& dist = m_distances[s];
        for (uint32_t u = 0; u < n; u++)
        {
            if (u == s || dist[u] == kInfinity)
            {
                continue;
            }
            int32_t parent = -1;
            double best = kInfinity;
            for (int slot = 0; slot < 4 && parent != static_cast<int32_t>(s); slot++)
            {
                const int32_t v = m_neighbours[u][slot];
                if (v == static_cast<int32_t>(s) && std::fabs(m_delays[u][slot] - dist[u]) <= 1e-12)
                {
                    parent = v;
                }
            }
            if (parent < 0)
            {
                for (int slot = 0; slot < 4; slot++)
                {
                    const int32_t v = m_neighbours[u][slot];
                    if (v < 0 || dist[v] == kInfinity)
                    {
                        continue;
                    }
                    const double cost = dist[v] + m_delays[u][slot];
                    if (cost < best - 1e-12 || (std::fabs(cost - best) <= 1e-12 && (parent < 0 || v < parent)))
                    {
                        best = cost;
                        parent = v;
                    }
                }
            }
            if (parent < 0)
            {
                continue;
            }
            for (int slot = 0; slot < 4; slot++)
            {
                if (m_neighbours[parent][slot] == static_cast<int32_t>(u))
                {
                    m_children[parent][s] |= static_cast<uint8_t>(1U << slot);
                    break;
                }
            }
        }
    }

    // valid only if every satellite is reachable over working links
    const auto reachable = static_cast<uint32_t>(
        std::count_if(m_distances[0].begin(), m_distances[0].end(), [](double d) { return d != kInfinity; }));
    m_valid = (n > 0 && numFailures == 0 && reachable == n && numLinks >= 2 * n);
    const bool report = (MpiInterface::GetSystemId() == 0);
    if (!m_valid)
    {
        if (report && ++m_numFailures <= kMaxReportedFailures)
        {
            std::printf("[SPT] t=%.3fs  incomplete topology (%u of %u satellites reachable, %u link failures), "
                        "retrying in %.1f s\n",
                        Simulator::Now().GetSeconds(), reachable, n, numFailures, kRetryInterval);
        }
        return;
    }
    if (report)
    {
        const double completion = *std::max_element(m_distances[0].begin(), m_distances[0].end());
        std::printf("[SPT] t=%.3fs  %u satellites, %u directed links, tree of sat 0 complete after %.1f ms, "
                    "duplicates so far %lu\n",
                    Simulator::Now().GetSeconds(), n, numLinks, completion * 1000.0,
                    static_cast<unsigned long>(m_numDuplicates));
        WriteTree(0);
    }
}

const char*
ShortestPathTrees::GetDirectionName(uint32_t from, uint32_t to) const
{
    const uint32_t s = m_satsPerOrbit;
    const uint32_t o = m_numOrbits;
    if (s == 0 || o == 0)
    {
        return "?";
    }
    const uint32_t fromOrbit = from / s;
    const uint32_t fromPos = from % s;
    const uint32_t toOrbit = to / s;
    const uint32_t toPos = to % s;
    if (fromOrbit == toOrbit)
    {
        if (toPos == (fromPos + 1) % s)
        {
            return "UP";
        }
        if (fromPos == (toPos + 1) % s)
        {
            return "DOWN";
        }
    }
    else if (fromPos == toPos)
    {
        if (toOrbit == (fromOrbit + 1) % o)
        {
            return "RIGHT";
        }
        if (fromOrbit == (toOrbit + 1) % o)
        {
            return "LEFT";
        }
    }
    return "?";
}

void
ShortestPathTrees::WriteTree(uint32_t origin)
{
    const uint32_t n = m_children.size();
    if (origin >= n || m_satsPerOrbit == 0)
    {
        return;
    }
    // parent, depth and link slot of every node in the tree of `origin` (breadth first)
    std::vector<int32_t> parent(n, -1);
    std::vector<int> parentSlot(n, -1);
    std::vector<uint32_t> depth(n, 0);
    std::vector<uint32_t> order{origin};
    for (std::size_t i = 0; i < order.size(); i++)
    {
        const uint32_t v = order[i];
        for (int slot = 0; slot < 4; slot++)
        {
            const int32_t u = m_neighbours[v][slot];
            if (((m_children[v][origin] >> slot) & 1U) && u >= 0 && parent[u] < 0 && u != static_cast<int32_t>(origin))
            {
                parent[u] = static_cast<int32_t>(v);
                parentSlot[u] = slot;
                depth[u] = depth[v] + 1;
                order.push_back(static_cast<uint32_t>(u));
            }
        }
    }

    const std::string file = SatelliteForwardingApp::OutputPath("packet_stats/spt_tree.csv");
    FILE* f = std::fopen(file.c_str(), m_csvCreated ? "a" : "w");
    if (!f)
    {
        return;
    }
    if (!m_csvCreated)
    {
        std::fputs("build_time,src,parent,child,parent_plane,parent_slot,child_plane,child_slot,tx_dir,"
                   "hop_delay_ms,depth,cum_delay_ms\n",
                   f);
        m_csvCreated = true;
    }
    const uint32_t s = m_satsPerOrbit;
    for (uint32_t u = 0; u < n; u++)
    {
        if (parent[u] < 0)
        {
            continue;
        }
        const auto v = static_cast<uint32_t>(parent[u]);
        std::fprintf(f,
                     "%.6f,%u,%u,%u,%u,%u,%u,%u,%s,%.6f,%u,%.6f\n",
                     Simulator::Now().GetSeconds(), origin, v, u, v / s, v % s, u / s, u % s,
                     GetDirectionName(v, u), m_delays[v][parentSlot[u]] * 1000.0, depth[u],
                     m_distances[origin][u] * 1000.0);
    }
    std::fclose(f);
}

/**
 * Shortest-path trees: every satellite forwards a broadcast to its children in
 * the delay-optimal tree of the origin (ShortestPathTrees). Floods while no
 * valid tree exists.
 */
class ShortestPathTreeBroadcast : public DeltaBroadcastAlgorithm
{
  public:
    void Init(SatelliteForwardingApp* app) override
    {
        DeltaBroadcastAlgorithm::Init(app);
        ShortestPathTrees::Get().Start(app->GetSatsPerOrbit(), app->GetNumOrbits());
    }

    std::vector<Ptr<NetDevice>> GetFirstHops() const override
    {
        std::vector<Ptr<NetDevice>> children;
        if (ShortestPathTrees::Get().GetChildren(m_app->GetSatId(), m_app->GetSatId(), children))
        {
            return children;
        }
        return DeltaBroadcastAlgorithm::GetFirstHops();
    }

    void Forward(Ptr<NetDevice> device, Ptr<Packet> packet, SatPacketHeader header) override
    {
        header.SetLastHop(m_app->GetSatId());
        std::vector<Ptr<NetDevice>> children;
        if (ShortestPathTrees::Get().GetChildren(m_app->GetSatId(), header.GetOrigin(), children))
        {
            for (Ptr<NetDevice> out : children)
            {
                Send(out, packet, header);
            }
            return;
        }
        for (Ptr<NetDevice> out : GetDevices())
        {
            if (out != device)
            {
                Send(out, packet, header);
            }
        }
    }

    void OnDuplicate(Ptr<NetDevice> device, const SatPacketHeader& header) override
    {
        ShortestPathTrees::Get().CountDuplicate();
    }
};

/// A broadcast algorithm that can be selected with --broadcastAlgorithm.
struct BroadcastAlgorithmEntry
{
    const char* name;
    std::unique_ptr<DeltaBroadcastAlgorithm> (*create)();
};

template <typename Algorithm>
std::unique_ptr<DeltaBroadcastAlgorithm>
CreateAlgorithm()
{
    return std::make_unique<Algorithm>();
}

/// All broadcast algorithms; the first one is the default.
const BroadcastAlgorithmEntry kBroadcastAlgorithms[] = {
    {"prune", &CreateAlgorithm<PruneBroadcast>},
    {"flood", &CreateAlgorithm<FloodBroadcast>},
    {"dim-order", &CreateAlgorithm<DimensionOrderBroadcast>},
    {"spt", &CreateAlgorithm<ShortestPathTreeBroadcast>},
};

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// DeltaBroadcastAlgorithm
// ─────────────────────────────────────────────────────────────────────────────

std::vector<Ptr<NetDevice>>
DeltaBroadcastAlgorithm::GetFirstHops() const
{
    const std::array<Ptr<NetDevice>, 4> devices = GetDevices();
    return {devices.begin(), devices.end()};
}

std::array<Ptr<NetDevice>, 4>
DeltaBroadcastAlgorithm::GetDevices() const
{
    return {m_app->GetDevUp(), m_app->GetDevDown(), m_app->GetDevLeft(), m_app->GetDevRight()};
}

int
DeltaBroadcastAlgorithm::GetPortIndex(Ptr<NetDevice> device) const
{
    const std::array<Ptr<NetDevice>, 4> devices = GetDevices();
    const auto it = std::find(devices.begin(), devices.end(), device);
    return (it == devices.end()) ? -1 : static_cast<int>(it - devices.begin());
}

void
DeltaBroadcastAlgorithm::Send(Ptr<NetDevice> device, Ptr<Packet> packet, const SatPacketHeader& header) const
{
    Ptr<Packet> copy = packet->Copy();
    copy->AddHeader(header);
    m_app->ForwardPacket(device, copy, SatelliteForwardingApp::kProtocolBroadcast);
}

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

std::vector<std::string>
RoutingRingSwitchDelta::GetBroadcastAlgorithmNames()
{
    std::vector<std::string> names;
    for (const BroadcastAlgorithmEntry& entry : kBroadcastAlgorithms)
    {
        names.emplace_back(entry.name);
    }
    return names;
}

RoutingRingSwitchDelta::RoutingRingSwitchDelta(const std::string& broadcastAlgorithm)
{
    const std::string name = broadcastAlgorithm.empty() ? kBroadcastAlgorithms[0].name : broadcastAlgorithm;
    for (const BroadcastAlgorithmEntry& entry : kBroadcastAlgorithms)
    {
        if (name == entry.name)
        {
            m_broadcast = entry.create();
        }
    }
    NS_ABORT_MSG_IF(!m_broadcast, "Unknown broadcast algorithm: " << name);
}

RoutingRingSwitchDelta::~RoutingRingSwitchDelta() = default;

// ─────────────────────────────────────────────────────────────────────────────
// Ring roles and switches
// ─────────────────────────────────────────────────────────────────────────────

void
RoutingRingSwitchDelta::Init(SatelliteForwardingApp* app)
{
    m_app = app;

    // exit positions at epoch 0: one ring satellite per orbit and ring
    const ConstellationConfig& config = m_app->GetConstellationConfig();
    const uint32_t satsPerOrbit = m_app->GetSatsPerOrbit();
    const uint32_t numOrbits = m_app->GetNumOrbits();
    m_upExit0.assign(numOrbits, -1);
    m_downExit0.assign(numOrbits, -1);
    for (uint32_t sat : config.ringUp)
    {
        const uint32_t orbit = sat / satsPerOrbit;
        if (orbit < numOrbits)
        {
            NS_ABORT_MSG_IF(m_upExit0[orbit] >= 0, "more than one ringUp satellite in orbit " << orbit);
            m_upExit0[orbit] = static_cast<int32_t>(sat % satsPerOrbit);
        }
    }
    for (uint32_t sat : config.ringDown)
    {
        const uint32_t orbit = sat / satsPerOrbit;
        if (orbit < numOrbits)
        {
            NS_ABORT_MSG_IF(m_downExit0[orbit] >= 0, "more than one ringDown satellite in orbit " << orbit);
            m_downExit0[orbit] = static_cast<int32_t>(sat % satsPerOrbit);
        }
    }

    Ptr<Node> left = PeerNode(m_app->GetDevLeft());
    Ptr<Node> right = PeerNode(m_app->GetDevRight());
    m_leftPeer = left ? left->GetId() : UINT32_MAX;
    m_rightPeer = right ? right->GetId() : UINT32_MAX;

    UpdateUpRole(0, false, false);
    UpdateDownRole(0);
    m_broadcast->Init(app);
}

bool
RoutingRingSwitchDelta::IsUpExit(uint32_t sat, uint32_t epoch) const
{
    const uint32_t satsPerOrbit = m_app->GetSatsPerOrbit();
    const uint32_t orbit = sat / satsPerOrbit;
    if (sat == UINT32_MAX || orbit >= m_upExit0.size() || m_upExit0[orbit] < 0)
    {
        return false;
    }
    return sat % satsPerOrbit ==
           (static_cast<uint32_t>(m_upExit0[orbit]) + satsPerOrbit - epoch % satsPerOrbit) % satsPerOrbit;
}

bool
RoutingRingSwitchDelta::IsDownExit(uint32_t sat, uint32_t epoch) const
{
    const uint32_t satsPerOrbit = m_app->GetSatsPerOrbit();
    const uint32_t orbit = sat / satsPerOrbit;
    if (sat == UINT32_MAX || orbit >= m_downExit0.size() || m_downExit0[orbit] < 0)
    {
        return false;
    }
    return sat % satsPerOrbit ==
           (static_cast<uint32_t>(m_downExit0[orbit]) + satsPerOrbit - epoch % satsPerOrbit) % satsPerOrbit;
}

void
RoutingRingSwitchDelta::UpdateUpRole(uint32_t epoch, bool announce, bool initiator)
{
    // Ring members (exit or entry) get the enlarged ring queues.
    const bool wasExit = m_upExit;
    const bool wasMember = m_app->IsRingUp();
    const bool entry = IsUpExit(m_leftPeer, epoch);
    m_upExit = IsUpExit(m_app->GetSatId(), epoch);
    m_app->SetRingUp(m_upExit || entry);
    if (m_app->IsRingUp() && !wasMember)
    {
        m_app->GrowRingQueues();
    }
    m_app->SetInsertionBlocked(UP, m_upExit && !entry); // kink exit
    if (!announce)
    {
        return;
    }
    if (m_upExit && !wasExit)
    {
        std::printf("[RingSwitchDelta] t=%.3fs  sat%u: %s ring-UP exit (epoch %u)\n",
                    Simulator::Now().GetSeconds(),
                    m_app->GetSatId(),
                    initiator ? "INITIATING switch, new" : "activated as new",
                    epoch);
        m_app->FillWithDummies(m_app->GetDevRight(), m_app->GetIslQueueSize() - 1);
    }
    else if (!m_upExit && wasExit)
    {
        std::printf("[RingSwitchDelta] t=%.3fs  sat%u: ring-UP exit retired (epoch %u)\n",
                    Simulator::Now().GetSeconds(), m_app->GetSatId(), epoch);
    }
}

void
RoutingRingSwitchDelta::UpdateDownRole(uint32_t epoch)
{
    // DOWN copies are routed by their own epoch (RouteDown): the role only
    // decides the ring membership and the insertion block of the kink exit.
    const bool wasMember = m_app->IsRingDown();
    const bool entry = IsDownExit(m_rightPeer, epoch);
    const bool exit = IsDownExit(m_app->GetSatId(), epoch);
    m_app->SetRingDown(exit || entry);
    if (m_app->IsRingDown() && !wasMember)
    {
        m_app->GrowRingQueues();
    }
    m_app->SetInsertionBlocked(DOWN, exit && !entry); // kink exit
}

void
RoutingRingSwitchDelta::InitiateRingUpSwitch()
{
    m_maxUpEpoch++;
    UpdateUpRole(m_maxUpEpoch, true, true);
    if (!m_upExit)
    {
        std::printf("[RingSwitchDelta] t=%.3fs  sat%u: WARNING ring-UP initiator is not the exit of epoch %u\n",
                    Simulator::Now().GetSeconds(), m_app->GetSatId(), m_maxUpEpoch);
    }
}

void
RoutingRingSwitchDelta::InitiateRingDownSwitch()
{
    m_maxDownEpoch++;
    m_downInitiatedEpoch = m_maxDownEpoch;
    UpdateDownRole(m_maxDownEpoch);
    std::printf("[RingSwitchDelta] t=%.3fs  sat%u: INITIATING ring-DOWN switch (epoch %u)\n",
                Simulator::Now().GetSeconds(), m_app->GetSatId(), m_maxDownEpoch);
    // the initiator must be the new exit of the kink orbit
    if (!IsDownExit(m_app->GetSatId(), m_maxDownEpoch) || IsDownExit(m_rightPeer, m_maxDownEpoch))
    {
        std::printf("[RingSwitchDelta] t=%.3fs  sat%u: WARNING ring-DOWN initiator is not the kink exit of "
                    "epoch %u\n",
                    Simulator::Now().GetSeconds(), m_app->GetSatId(), m_maxDownEpoch);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Routing tables. `device` is the device the copy arrived on: a copy that
// arrives on devUp was sent downwards, one that arrives on devDown upwards.
// ─────────────────────────────────────────────────────────────────────────────

Ptr<NetDevice>
RoutingRingSwitchDelta::RouteUp(Ptr<NetDevice> device, bool& leavesOrbit) const
{
    leavesOrbit = false;
    if (device == m_app->GetDevLeft())
    {
        return m_app->GetDevUp(); // enter this orbit
    }
    if (device == m_app->GetDevDown())
    {
        if (m_upExit)
        {
            leavesOrbit = true;
            return m_app->GetDevRight(); // lap done: to the next orbit
        }
        return m_app->GetDevUp();
    }
    if (device == m_app->GetDevUp())
    {
        return m_app->GetDevDown(); // reverse direction (not used by UP copies)
    }
    return nullptr;
}

Ptr<NetDevice>
RoutingRingSwitchDelta::RouteDown(Ptr<NetDevice> device, uint32_t& epoch)
{
    if (device == m_app->GetDevRight())
    {
        epoch = std::max(epoch, m_downInitiatedEpoch); // switch initiator: new epoch
        return m_app->GetDevDown();                    // enter this orbit
    }
    if (device == m_app->GetDevUp())
    {
        if (!IsDownExit(m_app->GetSatId(), epoch))
        {
            return m_app->GetDevDown();
        }
        if (epoch > m_downExitEpoch)
        {
            // first copy of a new epoch: the new ring ISL gets the queueing
            // delay of the old one
            m_downExitEpoch = epoch;
            std::printf("[RingSwitchDelta] t=%.3fs  sat%u: first ring-DOWN exit of epoch %u\n",
                        Simulator::Now().GetSeconds(), m_app->GetSatId(), epoch);
            m_app->FillWithDummies(m_app->GetDevLeft(), m_app->GetIslQueueSize() - 1);
        }
        return m_app->GetDevLeft(); // lap done: to the next orbit
    }
    if (device == m_app->GetDevDown())
    {
        return m_app->GetDevUp(); // reverse direction (not used by DOWN copies)
    }
    return nullptr;
}


// ─────────────────────────────────────────────────────────────────────────────
// Receiving
// ─────────────────────────────────────────────────────────────────────────────

bool
RoutingRingSwitchDelta::OnReceive(Ptr<NetDevice> device,
                                  Ptr<const Packet> packet,
                                  uint16_t protocol,
                                  const Address& sender)
{
    if (protocol != SatelliteForwardingApp::kProtocolData &&
        protocol != SatelliteForwardingApp::kProtocolBroadcast)
    {
        return false;
    }

    Ptr<Packet> copy = packet->Copy();
    SatPacketHeader header;
    copy->RemoveHeader(header);
    const direction_t direction = static_cast<direction_t>(header.GetDirection());

    if (direction == DUMMY)
    {
        return true;
    }
    // Expired broadcasts are dropped; expired stored copies are handled by
    // their origin (SatelliteForwardingApp::ReplaceExpiredOwnCopy).
    const bool expired = SatelliteForwardingApp::IsExpired(header);
    if (expired && direction != UP && direction != DOWN)
    {
        return true;
    }

    // own copy back at its origin
    if (header.GetOrigin() == m_app->GetSatId() && (direction == UP || direction == DOWN) && !expired)
    {
        if (SatelliteForwardingApp::PacketRecord* record = m_app->GetRecord(header.GetId()))
        {
            m_app->RecordReturn(record, direction);
            if (header.GetDupCode() != 0)
            {
                m_app->LogReassembly(header.GetDupCode(), header.GetOrigin(), header.GetId());
            }
        }
    }

    switch (direction)
    {
    case DOWN:
        ReceiveDown(device, copy, header);
        break;
    case UP:
        ReceiveUp(device, copy, header);
        break;
    case BROADCAST:
        if (protocol == SatelliteForwardingApp::kProtocolBroadcast)
        {
            ReceiveBroadcast(device, copy, header);
        }
        break;
    case BROADCAST_CONTROL:
        m_broadcast->OnControl(device, header);
        break;
    default:
        break;
    }
    return true;
}

void
RoutingRingSwitchDelta::ReceiveUp(Ptr<NetDevice> device, Ptr<Packet> packet, SatPacketHeader& header)
{
    if (header.GetEpoch() > m_maxUpEpoch)
    {
        m_maxUpEpoch = header.GetEpoch();
        UpdateUpRole(m_maxUpEpoch, true, false);
    }
    m_app->SetInsertionEpoch(UP, m_maxUpEpoch);

    if (m_app->ReplaceExpiredOwnCopy(header))
    {
        return;
    }
    if (header.GetDupCode() > 0 && m_app->CreateSecondCopy(DOWN, header, m_maxDownEpoch))
    {
        header.SetDupCode(0);
    }

    bool leavesOrbit = false;
    Ptr<NetDevice> out = RouteUp(device, leavesOrbit);
    if (!out)
    {
        m_app->LogUnroutable(kUnroutableUp, header.GetLastHop());
        return;
    }
    if (leavesOrbit)
    {
        header.SetEpoch(m_maxUpEpoch);
    }
    else if (out == m_app->GetDevUp())
    {
        m_app->RegulateStorageQueue(UP);
    }
    header.SetLastHop(m_app->GetSatId());
    packet->AddHeader(header);
    m_app->ForwardPacket(out, packet);
}

void
RoutingRingSwitchDelta::ReceiveDown(Ptr<NetDevice> device, Ptr<Packet> packet, SatPacketHeader& header)
{
    // Route first: the switch initiator gives the copies entering there the new epoch.
    uint32_t epoch = header.GetEpoch();
    Ptr<NetDevice> out = RouteDown(device, epoch);
    if (epoch > m_maxDownEpoch)
    {
        m_maxDownEpoch = epoch;
        UpdateDownRole(epoch);
    }
    m_app->SetInsertionEpoch(DOWN, epoch);

    if (m_app->ReplaceExpiredOwnCopy(header))
    {
        return;
    }
    if (header.GetDupCode() > 0 && m_app->CreateSecondCopy(UP, header, m_maxUpEpoch))
    {
        header.SetDupCode(0);
    }

    if (!out)
    {
        m_app->LogUnroutable(kUnroutableDown, header.GetLastHop());
        return;
    }
    if (out == m_app->GetDevDown())
    {
        m_app->RegulateStorageQueue(DOWN);
    }
    header.SetEpoch(epoch);
    header.SetLastHop(m_app->GetSatId());
    packet->AddHeader(header);
    m_app->ForwardPacket(out, packet);
}

void
RoutingRingSwitchDelta::ReceiveBroadcast(Ptr<NetDevice> device, Ptr<Packet> packet, SatPacketHeader& header)
{
    const uint16_t origin = header.GetOrigin();
    if (origin == m_app->GetSatId())
    {
        return;
    }
    if (m_broadcastFilter.SeenBefore(origin, header.GetId()))
    {
        m_broadcast->OnDuplicate(device, header);
        return;
    }
    if (header.GetDupCode() > 0)
    {
        m_app->LogBroadcast(false, origin, header.GetId());
    }
    m_broadcast->Forward(device, packet, header);
}

std::vector<Ptr<NetDevice>>
RoutingRingSwitchDelta::GetBroadcastFirstHops() const
{
    return m_broadcast->GetFirstHops();
}

} // namespace ns3
