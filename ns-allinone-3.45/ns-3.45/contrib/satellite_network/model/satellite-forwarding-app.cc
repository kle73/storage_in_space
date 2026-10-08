#include "satellite-forwarding-app.h"

#include "content/content-fill-double.h"
#include "routing/routing-ring-switch-walker-delta.h"
#include "routing/routing-ring-switch-walker-star.h"

#include "ns3/log.h"
#include "ns3/mpi-interface.h"
#include "ns3/node.h"
#include "ns3/point-to-point-laser-net-device.h"
#include "ns3/queue.h"
#include "ns3/rng-seed-manager.h"
#include "ns3/simulator.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <system_error>

NS_LOG_COMPONENT_DEFINE("SatelliteForwardingApp");

namespace ns3
{

namespace
{

/// Records are kept this long after their time to live has expired [s].
constexpr double kRecordGrace = 0.1;
/// A packet counts as missing if none of its copies passed its origin for this
/// long [s], and for at least twice the longest round-trip time measured.
constexpr double kMissingAfter = 6.2;
/// Interval of the queue and packet statistics [s].
constexpr double kStatisticsInterval = 0.01;
/// Interval of the queue limit adjustment [ms].
constexpr int64_t kQueueAdjustIntervalMs = 1;
/// Switch guard (IsInSwitchGuard): from this long before a ring switch until
/// this long after it [s]. Closed rings need longer, the new DOWN epoch
/// spreads one orbit lap at a time.
constexpr double kSwitchGuardBefore = 1.0;
constexpr double kSwitchGuardAfter = 3.0;
constexpr double kSwitchGuardAfterClosedRing = 10.0;
/// Top-ups of at least this many dummies are printed.
constexpr uint64_t kTopUpLogThreshold = 100;
/// Broadcast duplicate suppression: ids per origin in the sliding window.
constexpr uint32_t kBroadcastWindow = 32768;

std::string&
OutputDirStorage()
{
    static std::string dir = "mysim_results";
    return dir;
}

Ptr<PointToPointLaserNetDevice>
Laser(Ptr<NetDevice> device)
{
    return DynamicCast<PointToPointLaserNetDevice>(device);
}

void
SetQueueLimit(Ptr<Queue<Packet>> queue, uint64_t packets)
{
    queue->SetMaxSize(QueueSize(std::to_string(packets) + "p"));
}

/// Raises the limit of the data and the broadcast queue of `device` to `packets`.
void
GrowQueues(Ptr<NetDevice> device, uint64_t packets)
{
    for (Ptr<Queue<Packet>> q : {Laser(device)->GetQueue(), Laser(device)->GetBroadcastQueue()})
    {
        if (q->GetMaxSize().GetValue() < packets)
        {
            SetQueueLimit(q, packets);
        }
    }
}

/// Appends one line to `file`.
template <typename... Args>
void
AppendLine(const std::string& file, const char* format, Args... args)
{
    FILE* f = std::fopen(file.c_str(), "a");
    if (f)
    {
        std::fprintf(f, format, args...);
        std::fclose(f);
    }
}

/// Creates `file` with the header line `header` (empty: empty file).
void
CreateFile(const std::string& file, const char* header)
{
    FILE* f = std::fopen(file.c_str(), "w");
    if (f)
    {
        std::fputs(header, f);
        std::fclose(f);
    }
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// SatPacketHeader
// ─────────────────────────────────────────────────────────────────────────────

NS_OBJECT_ENSURE_REGISTERED(SatPacketHeader);

TypeId
SatPacketHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::SatPacketHeader")
                            .SetParent<Header>()
                            .SetGroupName("SatelliteNetwork")
                            .AddConstructor<SatPacketHeader>();
    return tid;
}

TypeId
SatPacketHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
SatPacketHeader::GetSerializedSize() const
{
    return 42;
}

void
SatPacketHeader::Serialize(Buffer::Iterator start) const
{
    start.WriteHtonU32(m_id);
    start.WriteHtonU16(m_origin);
    start.WriteHtonU64(m_creationTimeMs);
    start.WriteHtonU64(m_ttlS);
    start.WriteHtonU16(m_direction);
    start.WriteHtonU32(m_epoch);
    start.WriteHtonU32(m_objectId);
    start.WriteHtonU32(m_fragmentId);
    start.WriteHtonU32(m_lastHop);
    start.WriteHtonU16(m_dupCode);
}

uint32_t
SatPacketHeader::Deserialize(Buffer::Iterator start)
{
    m_id = start.ReadNtohU32();
    m_origin = start.ReadNtohU16();
    m_creationTimeMs = start.ReadNtohU64();
    m_ttlS = start.ReadNtohU64();
    m_direction = start.ReadNtohU16();
    m_epoch = start.ReadNtohU32();
    m_objectId = start.ReadNtohU32();
    m_fragmentId = start.ReadNtohU32();
    m_lastHop = start.ReadNtohU32();
    m_dupCode = start.ReadNtohU16();
    return GetSerializedSize();
}

void
SatPacketHeader::Print(std::ostream& os) const
{
    os << "direction=" << m_direction << " id=" << m_id << " origin=" << m_origin
       << " epoch=" << m_epoch;
}

// ─────────────────────────────────────────────────────────────────────────────
// BroadcastFilter
// ─────────────────────────────────────────────────────────────────────────────

bool
BroadcastFilter::SeenBefore(uint16_t origin, uint32_t id)
{
    if (m_windows.size() <= origin)
    {
        m_windows.resize(origin + 1);
    }
    Window& window = m_windows[origin];
    if (!window.initialized)
    {
        window.bits.assign(kBroadcastWindow / 64, 0);
        window.base = (id >= kBroadcastWindow / 2 ? id - kBroadcastWindow / 2 : 0) & ~63U;
        window.initialized = true;
    }
    if (id < window.base)
    {
        return true;
    }
    if (id >= window.base + kBroadcastWindow)
    {
        // slide the window forward
        const uint32_t newBase = (id - kBroadcastWindow + 1) & ~63U;
        const uint32_t shift = (newBase - window.base) / 64;
        if (shift >= window.bits.size())
        {
            std::fill(window.bits.begin(), window.bits.end(), 0);
        }
        else
        {
            std::move(window.bits.begin() + shift, window.bits.end(), window.bits.begin());
            std::fill(window.bits.end() - shift, window.bits.end(), 0);
        }
        window.base = newBase;
    }
    const uint32_t offset = id - window.base;
    uint64_t& word = window.bits[offset / 64];
    const uint64_t mask = 1ULL << (offset % 64);
    if (word & mask)
    {
        return true;
    }
    word |= mask;
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Output folder
// ─────────────────────────────────────────────────────────────────────────────

void
SatelliteForwardingApp::SetOutputDir(const std::string& dir)
{
    std::string d = dir.empty() ? std::string(".") : dir;
    while (d.size() > 1 && d.back() == '/')
    {
        d.pop_back();
    }
    OutputDirStorage() = d;
}

std::string
SatelliteForwardingApp::GetOutputDir()
{
    return OutputDirStorage();
}

std::string
SatelliteForwardingApp::OutputPath(const std::string& relPath)
{
    namespace fs = std::filesystem;
    const fs::path path = fs::path(OutputDirStorage()) / relPath;
    if (path.has_parent_path())
    {
        // Several MPI ranks may create the folder at the same time: only check
        // that it exists afterwards.
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        if (!fs::is_directory(path.parent_path(), ec))
        {
            std::cerr << "[SatelliteForwardingApp] WARNING: cannot create output folder "
                      << path.parent_path().string() << std::endl;
        }
    }
    return path.string();
}

// ─────────────────────────────────────────────────────────────────────────────
// Setup and start
// ─────────────────────────────────────────────────────────────────────────────

NS_OBJECT_ENSURE_REGISTERED(SatelliteForwardingApp);

TypeId
SatelliteForwardingApp::GetTypeId()
{
    static TypeId tid = TypeId("ns3::SatelliteForwardingApp")
                            .SetParent<Application>()
                            .SetGroupName("SatelliteNetwork")
                            .AddConstructor<SatelliteForwardingApp>();
    return tid;
}

SatelliteForwardingApp::SatelliteForwardingApp() = default;

SatelliteForwardingApp::~SatelliteForwardingApp() = default;

void
SatelliteForwardingApp::Setup(const SatelliteForwardingAppParams& params)
{
    m_satsPerOrbit = params.satellitesPerOrbit;
    m_numSatellites = params.numSatellites;
    m_numOrbits = m_numSatellites / m_satsPerOrbit;
    m_devUp = params.devUp;
    m_devDown = params.devDown;
    m_devRight = params.devRight;
    m_devLeft = params.devLeft;
    m_constellation = params.constellation;
    m_maxQueueFillLevel = params.maxQueueFillLevel;
    m_objectSize = params.objectSize;
    m_objectTtl = params.objectTtl;
    m_runNumber = params.runNumber;

    if (params.routingAlgorithm == "ring-switch-walker-star")
    {
        m_routing = std::make_unique<RoutingRingSwitchStar>();
    }
    else if (params.routingAlgorithm == "ring-switch-walker-delta")
    {
        m_routing = std::make_unique<RoutingRingSwitchDelta>(params.broadcastAlgorithm);
    }
    else
    {
        NS_FATAL_ERROR("Unknown routing algorithm: " << params.routingAlgorithm);
    }

    if (params.contentGeneration == "fill-double")
    {
        m_content = std::make_unique<ContentFillDouble>(ContentFillDouble::Mode::STORAGE);
    }
    else if (params.contentGeneration == "broadcast")
    {
        m_content = std::make_unique<ContentFillDouble>(ContentFillDouble::Mode::BROADCAST);
    }
    else
    {
        NS_FATAL_ERROR("Unknown content generation: " << params.contentGeneration);
    }
}

void
SatelliteForwardingApp::StartApplication()
{
    NS_ABORT_MSG_IF(!m_routing, "SatelliteForwardingApp::Setup() was not called");

    const std::string run = std::to_string(m_runNumber);
    m_queueStatsFile = OutputPath("queue_stats/experiment4/queue_statisticsS" + run + ".csv");
    m_packetStatsFile = OutputPath("packet_stats/experiment4/packet_statisticsS" + run + ".txt");
    m_reassemblyFile = OutputPath("packet_stats/experiment4/packet_reassembleS" + run + ".csv");
    m_broadcastFile = OutputPath("broadcast/experiment4/broadcast_statsS" + run + ".csv");
    m_insertionFile = OutputPath("object_duplication/experiment4/obj_injectS" + run + ".csv");
    m_duplicationFile = OutputPath("object_duplication/experiment4/obj_dupS" + run + ".csv");
    m_unroutableFile = OutputPath("packet_stats/packet_monitoring_dataFix.csv");

    m_satId = GetNode()->GetId();
    m_ringUp = m_constellation.ringUp.count(m_satId) > 0;
    m_ringDown = m_constellation.ringDown.count(m_satId) > 0;
    m_seamLeft = m_constellation.seamLeft.count(m_satId) > 0;
    m_seamRight = m_constellation.seamRight.count(m_satId) > 0;

    const uint32_t rank = MpiInterface::GetSystemId();
    if (rank == 0 && m_satId == 0)
    {
        CreateFile(m_queueStatsFile,
                   "node,time,up,down,left,right,up_bc,down_bc,left_bc,right_bc,timestamp,queue\n");
        CreateFile(m_packetStatsFile, "");
        CreateFile(m_reassemblyFile, "node,time,type,origin,pkt_id\n");
        CreateFile(m_broadcastFile, "is_sender,node,origin,id,time\n");
        CreateFile(m_insertionFile, "time_s,origin,obj_id,num_frags,mode\n");
        CreateFile(m_duplicationFile, "time_s,relay_sat,origin,obj_id,frag_id\n");
        CreateFile(m_unroutableFile, "node,last_hop,time,code\n");
    }

    RngSeedManager::SetSeed(rank * m_satId + m_satId - rank + 20 + m_runNumber);
    RngSeedManager::SetRun(rank + 1);
    m_random = CreateObject<UniformRandomVariable>();

    // The configured queue size is the ISL queue size; every queue gets some
    // headroom above it.
    m_islQueueSize = Laser(m_devUp)->GetQueue()->GetMaxSize().GetValue();
    NS_ABORT_MSG_IF(m_islQueueSize < m_objectSize, "Objects do not fit into the ISL queues");
    for (Ptr<NetDevice> device : {m_devUp, m_devDown, m_devLeft, m_devRight})
    {
        SetQueueLimit(Laser(device)->GetQueue(), m_islQueueSize + kQueueHeadroom);
        SetQueueLimit(Laser(device)->GetBroadcastQueue(), m_islQueueSize + kQueueHeadroom);
    }

    m_routing->Init(this);
    m_content->Init(this);
    auto receive = MakeCallback(&SatelliteForwardingApp::ReceiveFromDevice, this);
    for (Ptr<NetDevice> device : {m_devUp, m_devDown, m_devRight, m_devLeft})
    {
        device->SetReceiveCallback(receive);
    }

    Simulator::Schedule(Seconds(kStatisticsInterval), &SatelliteForwardingApp::WriteQueueStatistics, this);
    Simulator::Schedule(Seconds(0.1), &SatelliteForwardingApp::WritePacketStatistics, this);
    m_content->Generate();
    Simulator::Schedule(MilliSeconds(kQueueAdjustIntervalMs), &SatelliteForwardingApp::AdjustQueueLimits, this);
}

bool
SatelliteForwardingApp::ReceiveFromDevice(Ptr<NetDevice> device,
                                          Ptr<const Packet> packet,
                                          uint16_t protocol,
                                          const Address& sender)
{
    return m_routing->OnReceive(device, packet, protocol, sender);
}

// ─────────────────────────────────────────────────────────────────────────────
// Queues and forwarding
// ─────────────────────────────────────────────────────────────────────────────

uint64_t
SatelliteForwardingApp::GetQueueOccupancy(Ptr<NetDevice> device)
{
    return Laser(device)->GetQueue()->GetCurrentSize().GetValue();
}

void
SatelliteForwardingApp::ForwardPacket(Ptr<NetDevice> outDev, Ptr<Packet> packet, uint16_t protocol)
{
    NS_ASSERT_MSG(outDev == m_devUp || outDev == m_devDown || outDev == m_devLeft ||
                      outDev == m_devRight,
                  "ForwardPacket: unknown device on sat " << m_satId);

    // The device picks the queue from the protocol number: check that queue.
    Ptr<Queue<Packet>> queue = (protocol == kProtocolBroadcast) ? Laser(outDev)->GetBroadcastQueue()
                                                                : Laser(outDev)->GetQueue();
    if (queue->GetCurrentSize().GetValue() < queue->GetMaxSize().GetValue())
    {
        outDev->Send(packet, outDev->GetBroadcast(), protocol);
    }
    else
    {
        m_numDropped++;
    }
}

void
SatelliteForwardingApp::FillWithDummies(Ptr<NetDevice> device, uint64_t level)
{
    if ((device == m_devLeft && m_seamLeft) || (device == m_devRight && m_seamRight))
    {
        return;
    }
    for (uint64_t n = GetQueueOccupancy(device); n < level; n++)
    {
        SatPacketHeader header;
        header.SetDirection(DUMMY);
        Ptr<Packet> packet = Create<Packet>(kPayloadSize);
        packet->AddHeader(header);
        device->Send(packet, device->GetBroadcast(), kProtocolData);
    }
}

std::vector<Ptr<NetDevice>>
SatelliteForwardingApp::GetBroadcastFirstHops() const
{
    return m_routing->GetBroadcastFirstHops();
}

void
SatelliteForwardingApp::GrowRingQueues()
{
    const uint64_t limit = m_islQueueSize + kRingQueueHeadroom;
    if (m_ringUp)
    {
        GrowQueues(m_devUp, limit);
        GrowQueues(m_devLeft, limit);
    }
    if (m_ringDown)
    {
        GrowQueues(m_devDown, limit);
        GrowQueues(m_devRight, limit);
    }
}

void
SatelliteForwardingApp::AdjustQueueLimits()
{
    // A queue that is no longer a ring queue keeps its enlarged limit until it
    // has drained to the ISL queue size: the traffic through it is a saturated
    // stream, so a limit equal to the backlog would drop packets instead of
    // letting the backlog drain. Nothing is shrunk around a ring switch, when
    // a retired ring satellite may still receive the end of the old stream.
    if (m_ringUp || m_ringDown)
    {
        GrowRingQueues();
    }
    else if (!IsInSwitchGuard())
    {
        const uint64_t limit = m_islQueueSize + kQueueHeadroom;
        for (Ptr<NetDevice> device : {m_devUp, m_devDown, m_devLeft, m_devRight})
        {
            for (Ptr<Queue<Packet>> q : {Laser(device)->GetQueue(), Laser(device)->GetBroadcastQueue()})
            {
                if (q->GetMaxSize().GetValue() > limit && q->GetCurrentSize().GetValue() <= m_islQueueSize)
                {
                    SetQueueLimit(q, limit);
                }
            }
        }
    }
    Simulator::Schedule(MilliSeconds(kQueueAdjustIntervalMs), &SatelliteForwardingApp::AdjustQueueLimits, this);
}

// ─────────────────────────────────────────────────────────────────────────────
// Storage levels
// ─────────────────────────────────────────────────────────────────────────────

Ptr<NetDevice>
SatelliteForwardingApp::GetStorageDevice(direction_t direction) const
{
    return direction == UP ? m_devUp : m_devDown;
}

SatelliteForwardingApp::StorageLevel&
SatelliteForwardingApp::GetStorageLevel(direction_t direction)
{
    return m_storageLevels[direction == DOWN ? 1 : 0];
}

const SatelliteForwardingApp::StorageLevel&
SatelliteForwardingApp::GetStorageLevel(direction_t direction) const
{
    return m_storageLevels[direction == DOWN ? 1 : 0];
}

bool
SatelliteForwardingApp::IsLevelActive(direction_t direction) const
{
    return GetStorageLevel(direction).active;
}

void
SatelliteForwardingApp::ActivateLevelIfFull(direction_t direction)
{
    StorageLevel& storage = GetStorageLevel(direction);
    if (!storage.active && GetQueueOccupancy(GetStorageDevice(direction)) + m_objectSize > GetFillLevel())
    {
        storage.active = true;
        storage.level = static_cast<int64_t>(GetFillLevel());
    }
}

void
SatelliteForwardingApp::RegulateStorageQueue(direction_t direction)
{
    ActivateLevelIfFull(direction);
    const StorageLevel& storage = GetStorageLevel(direction);
    if (!storage.active || storage.level <= 0)
    {
        return;
    }
    const uint64_t level = static_cast<uint64_t>(storage.level);
    Ptr<NetDevice> device = GetStorageDevice(direction);
    const uint64_t before = GetQueueOccupancy(device);
    if (before >= level)
    {
        return;
    }
    FillWithDummies(device, level);
    if (level - before >= kTopUpLogThreshold)
    {
        std::printf("[SatelliteForwardingApp] t=%.3fs  sat%u: %s queue topped up %lu -> %lu (dummies)\n",
                    Simulator::Now().GetSeconds(),
                    m_satId,
                    direction == UP ? "UP" : "DOWN",
                    static_cast<unsigned long>(before),
                    static_cast<unsigned long>(GetQueueOccupancy(device)));
    }
}

uint64_t
SatelliteForwardingApp::GetStorageSpace(direction_t direction) const
{
    if (IsInsertionBlocked(direction))
    {
        return 0;
    }
    const int64_t fill = static_cast<int64_t>(GetFillLevel());
    const int64_t occupancy = static_cast<int64_t>(GetQueueOccupancy(GetStorageDevice(direction)));
    int64_t space = fill - occupancy;
    const StorageLevel& storage = GetStorageLevel(direction);
    if (storage.active)
    {
        space = std::min(space, fill - storage.level);
    }
    return static_cast<uint64_t>(std::max<int64_t>(space, 0));
}

SatelliteForwardingApp::Admission
SatelliteForwardingApp::AdmitObject(uint32_t numPackets)
{
    if (IsInSwitchGuard())
    {
        return Admission::NONE;
    }
    ActivateLevelIfFull(UP);
    ActivateLevelIfFull(DOWN);
    const bool up = GetStorageSpace(UP) >= numPackets;
    const bool down = GetStorageSpace(DOWN) >= numPackets;
    if ((!up && !down) || (IsLevelActive(UP) && IsLevelActive(DOWN) && !(up && down)))
    {
        return Admission::NONE;
    }
    for (direction_t direction : {UP, DOWN})
    {
        StorageLevel& storage = GetStorageLevel(direction);
        if ((direction == UP ? up : down) && storage.active)
        {
            storage.level += static_cast<int64_t>(numPackets);
        }
    }
    if (up && down)
    {
        return Admission::BOTH;
    }
    return up ? Admission::UP_ONLY : Admission::DOWN_ONLY;
}

bool
SatelliteForwardingApp::IsExpired(const SatPacketHeader& header)
{
    return static_cast<uint64_t>(Simulator::Now().GetMilliSeconds()) - header.GetCreationTimeMs() >
           header.GetTtl() * 1000;
}

bool
SatelliteForwardingApp::ReplaceExpiredOwnCopy(SatPacketHeader& header)
{
    const direction_t direction = static_cast<direction_t>(header.GetDirection());
    if (header.GetOrigin() != m_satId || (direction != UP && direction != DOWN) || !IsExpired(header) ||
        IsInSwitchGuard())
    {
        return false;
    }
    // In place: the stream keeps its number of packets, the queues are not affected.
    if (m_replacementSource && m_replacementSource(direction, header))
    {
        return false;
    }
    StorageLevel& storage = GetStorageLevel(direction);
    if (storage.active)
    {
        storage.level--;
    }
    return true;
}

bool
SatelliteForwardingApp::CreateSecondCopy(direction_t direction, const SatPacketHeader& header, uint32_t epoch)
{
    // Once the level of a queue is active, its data only changes by admission
    // and deletion at the origin.
    Ptr<NetDevice> device = GetStorageDevice(direction);
    if (IsInsertionBlocked(direction) || IsLevelActive(direction) || IsInSwitchGuard() || IsExpired(header) ||
        GetQueueOccupancy(device) >= GetFillLevel())
    {
        return false;
    }
    SatPacketHeader copy = header;
    copy.SetDirection(direction);
    copy.SetEpoch(epoch);
    copy.SetLastHop(m_satId);
    copy.SetDupCode(0);
    Ptr<Packet> packet = Create<Packet>(kPayloadSize);
    packet->AddHeader(copy);
    device->Send(packet, device->GetBroadcast(), kProtocolData);
    LogDuplication(header.GetOrigin(), header.GetObjectId(), header.GetFragmentId());
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Ring roles and switches
// ─────────────────────────────────────────────────────────────────────────────

bool
SatelliteForwardingApp::IsInsertionBlocked(direction_t direction) const
{
    return m_insertionBlocked[direction == DOWN ? 1 : 0];
}

void
SatelliteForwardingApp::SetInsertionBlocked(direction_t direction, bool blocked)
{
    m_insertionBlocked[direction == DOWN ? 1 : 0] = blocked;
}

uint32_t
SatelliteForwardingApp::GetInsertionEpoch(direction_t direction) const
{
    return m_insertionEpoch[direction == DOWN ? 1 : 0];
}

void
SatelliteForwardingApp::SetInsertionEpoch(direction_t direction, uint32_t epoch)
{
    m_insertionEpoch[direction == DOWN ? 1 : 0] = epoch;
}

void
SatelliteForwardingApp::TriggerRingUpSwitch()
{
    m_routing->InitiateRingUpSwitch();
}

void
SatelliteForwardingApp::TriggerRingDownSwitch()
{
    m_routing->InitiateRingDownSwitch();
}

bool
SatelliteForwardingApp::IsInSwitchGuard() const
{
    const double now = Simulator::Now().GetSeconds();
    const double after = m_constellation.closedRing ? kSwitchGuardAfterClosedRing : kSwitchGuardAfter;
    for (double t : m_switchTimes)
    {
        if (now >= t - kSwitchGuardBefore && now <= t + after)
        {
            return true;
        }
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Own packets
// ─────────────────────────────────────────────────────────────────────────────

uint32_t
SatelliteForwardingApp::AllocatePacketId()
{
    const double now = Simulator::Now().GetSeconds();
    m_records.push_back(PacketRecord{now, now});
    return m_nextPacketId++;
}

SatelliteForwardingApp::PacketRecord*
SatelliteForwardingApp::GetRecord(uint32_t id)
{
    if (id < m_firstRecordId || id - m_firstRecordId >= m_records.size())
    {
        m_numStaleRecords++;
        return nullptr;
    }
    return &m_records[id - m_firstRecordId];
}

void
SatelliteForwardingApp::RecordReturn(PacketRecord* record, direction_t direction)
{
    const double now = Simulator::Now().GetSeconds();
    double& last = (direction == UP) ? record->lastSeenUp : record->lastSeenDown;
    if (last > 0)
    {
        m_rttSum += now - last;
        m_rttCount++;
        m_maxRtt = std::max(m_maxRtt, now - last);
    }
    last = now;
    record->lastSeen = now;
}

void
SatelliteForwardingApp::EvictExpiredRecords()
{
    // Ids are allocated in time order, so expired records form a prefix.
    const double now = Simulator::Now().GetSeconds();
    while (!m_records.empty() && now - m_records.front().created > m_objectTtl + kRecordGrace)
    {
        m_records.pop_front();
        m_firstRecordId++;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Statistics and logs
// ─────────────────────────────────────────────────────────────────────────────

void
SatelliteForwardingApp::WriteQueueStatistics()
{
    static const time_t wallClockStart = std::time(nullptr);
    auto data = [](Ptr<NetDevice> d) {
        return static_cast<unsigned long>(Laser(d)->GetQueue()->GetCurrentSize().GetValue());
    };
    auto broadcast = [](Ptr<NetDevice> d) {
        return static_cast<unsigned long>(Laser(d)->GetBroadcastQueue()->GetCurrentSize().GetValue());
    };
    // last column: unused, always 0 (kept for the file format)
    AppendLine(m_queueStatsFile,
               "%u,%lf,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lf,%lu\n",
               GetNode()->GetId(),
               Simulator::Now().GetSeconds(),
               data(m_devUp), data(m_devDown), data(m_devLeft), data(m_devRight),
               broadcast(m_devUp), broadcast(m_devDown), broadcast(m_devLeft), broadcast(m_devRight),
               static_cast<double>(std::time(nullptr) - wallClockStart),
               0UL);
    Simulator::Schedule(Seconds(kStatisticsInterval), &SatelliteForwardingApp::WriteQueueStatistics, this);
}

void
SatelliteForwardingApp::WritePacketStatistics()
{
    EvictExpiredRecords();

    const double now = Simulator::Now().GetSeconds();
    const double missingAfter = std::max(kMissingAfter, 2.0 * m_maxRtt);
    uint32_t inSystem = 0;
    uint32_t missing = 0;
    for (const PacketRecord& record : m_records)
    {
        if (now - record.lastSeen <= missingAfter)
        {
            inSystem++;
        }
        else
        {
            missing++;
        }
    }

    if (m_rttCount > 0)
    {
        m_meanRtt = m_rttSum / m_rttCount;
        m_rttSum = 0.0;
        m_rttCount = 0;
    }

    // node, time, packets in the system, packet ids allocated, mean RTT,
    // dropped (cumulative), missing, stale records
    AppendLine(m_packetStatsFile,
               "%u,%lf,%u,%u,%lf,%u,%u,%u\n",
               GetNode()->GetId(),
               now,
               inSystem,
               m_nextPacketId,
               m_meanRtt,
               m_numDropped,
               missing,
               static_cast<uint32_t>(m_numStaleRecords));
    Simulator::Schedule(Seconds(kStatisticsInterval), &SatelliteForwardingApp::WritePacketStatistics, this);
}

void
SatelliteForwardingApp::LogUnroutable(int code, uint32_t lastHop)
{
    m_numDropped++;
    AppendLine(m_unroutableFile, "%u,%u,%lf,%d\n", m_satId, lastHop, Simulator::Now().GetSeconds(), code);
}

void
SatelliteForwardingApp::LogObjectInsertion(uint32_t objectId, uint32_t numFragments, int mode)
{
    AppendLine(m_insertionFile,
               "%lf,%u,%u,%u,%d\n",
               Simulator::Now().GetSeconds(), m_satId, objectId, numFragments, mode);
}

void
SatelliteForwardingApp::LogDuplication(uint16_t origin, uint32_t objectId, uint32_t fragmentId)
{
    AppendLine(m_duplicationFile,
               "%lf,%u,%u,%u,%u\n",
               Simulator::Now().GetSeconds(), m_satId, static_cast<uint32_t>(origin), objectId, fragmentId);
}

void
SatelliteForwardingApp::LogReassembly(uint16_t dupCode, uint16_t origin, uint32_t id)
{
    AppendLine(m_reassemblyFile,
               "%u,%lf,%u,%u,%u\n",
               m_satId, Simulator::Now().GetSeconds(), static_cast<uint32_t>(dupCode),
               static_cast<uint32_t>(origin), id);
}

void
SatelliteForwardingApp::LogBroadcast(bool isSender, uint16_t origin, uint32_t id)
{
    AppendLine(m_broadcastFile,
               "%d,%u,%u,%u,%lf\n",
               isSender ? 1 : 0, m_satId, static_cast<uint32_t>(origin), id, Simulator::Now().GetSeconds());
}

} // namespace ns3
