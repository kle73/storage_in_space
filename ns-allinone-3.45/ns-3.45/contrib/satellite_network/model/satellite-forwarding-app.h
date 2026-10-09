#ifndef SATELLITE_FORWARDING_APP_H
#define SATELLITE_FORWARDING_APP_H

#include "constellation-config.h"
#include "strategy-interfaces.h"

#include "ns3/application.h"
#include "ns3/header.h"
#include "ns3/net-device.h"
#include "ns3/packet.h"
#include "ns3/random-variable-stream.h"

#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ns3
{

/**
 * Header of every packet sent by the storage applications (data copies,
 * dummies and broadcasts). Wire size: 42 bytes.
 */
class SatPacketHeader : public Header
{
  public:
    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;
    uint32_t GetSerializedSize() const override;

    void SetId(uint32_t id) { m_id = id; }
    void SetOrigin(uint16_t origin) { m_origin = origin; }
    void SetCreationTimeMs(uint64_t timeMs) { m_creationTimeMs = timeMs; }
    void SetTtl(uint64_t ttlS) { m_ttlS = ttlS; }
    void SetDirection(uint16_t direction) { m_direction = direction; }
    void SetEpoch(uint32_t epoch) { m_epoch = epoch; }
    void SetObjectId(uint32_t objectId) { m_objectId = objectId; }
    void SetFragmentId(uint32_t fragmentId) { m_fragmentId = fragmentId; }
    void SetLastHop(uint32_t lastHop) { m_lastHop = lastHop; }
    void SetDupCode(uint16_t dupCode) { m_dupCode = dupCode; }

    /// Packet id, unique per origin satellite (shared by both copies of a fragment).
    uint32_t GetId() const { return m_id; }
    /// Satellite that created the packet and keeps its record.
    uint16_t GetOrigin() const { return m_origin; }
    uint64_t GetCreationTimeMs() const { return m_creationTimeMs; }
    /// Time to live [s].
    uint64_t GetTtl() const { return m_ttlS; }
    /// One of direction_t.
    uint16_t GetDirection() const { return m_direction; }
    /// Ring-switch epoch of the ring the copy travels in.
    uint32_t GetEpoch() const { return m_epoch; }
    uint32_t GetObjectId() const { return m_objectId; }
    uint32_t GetFragmentId() const { return m_fragmentId; }
    /// Satellite that sent the packet (broadcast algorithms reuse it as hop counter).
    uint32_t GetLastHop() const { return m_lastHop; }
    /// > 0: the second copy of this fragment still has to be created
    /// (broadcasts: 1 = sampled for the delivery statistics).
    uint16_t GetDupCode() const { return m_dupCode; }

  private:
    uint32_t m_id{0};
    uint16_t m_origin{0};
    uint64_t m_creationTimeMs{0};
    uint64_t m_ttlS{0};
    uint16_t m_direction{0};
    uint32_t m_epoch{0};
    uint32_t m_objectId{0};
    uint32_t m_fragmentId{0};
    uint32_t m_lastHop{0};
    uint16_t m_dupCode{0};
};

/**
 * Broadcast duplicate suppression: remembers the ids received from every
 * origin in a sliding window of the newest ids. Ids older than the window
 * count as seen (their broadcasts have expired).
 */
class BroadcastFilter
{
  public:
    /// True if (origin, id) was seen before; otherwise it is marked as seen.
    bool SeenBefore(uint16_t origin, uint32_t id);

  private:
    struct Window
    {
        uint32_t base{0}; ///< lowest id in the window, multiple of 64
        bool initialized{false};
        std::vector<uint64_t> bits;
    };

    std::vector<Window> m_windows; ///< indexed by origin
};

/**
 * Parameters of one satellite application (see SatelliteForwardingApp::Setup).
 */
struct SatelliteForwardingAppParams
{
    uint32_t satellitesPerOrbit{0};
    uint32_t numSatellites{0};
    Ptr<NetDevice> devUp;    ///< next satellite in the orbit
    Ptr<NetDevice> devDown;  ///< previous satellite in the orbit
    Ptr<NetDevice> devRight; ///< satellite in the next orbit
    Ptr<NetDevice> devLeft;  ///< satellite in the previous orbit
    std::string routingAlgorithm;  ///< ring-switch-walker-star | ring-switch-walker-delta
    std::string contentGeneration; ///< fill-double | broadcast
    std::string broadcastAlgorithm{"prune"}; ///< walker-delta only, see RoutingRingSwitchDelta
    ConstellationConfig constellation;
    uint32_t maxQueueFillLevel{100}; ///< fill level of the storage queues, percent of the ISL queue size
    uint32_t objectSize{10};         ///< packets per object
    uint64_t objectTtl{10000};       ///< time to live of stored objects [s]
    uint32_t runNumber{0};
};

/**
 * Application running on every satellite.
 *
 * It owns the four ISL devices, creates the routing and the content strategy
 * and provides what both share: packet forwarding, queue sizes, the storage
 * levels, the ring roles of the satellite, the records of its own packets, the
 * statistics and the output files.
 *
 * Storage levels: the storage queue of a direction is the data queue of devUp
 * (UP copies) or of devDown (DOWN copies). Its level becomes active, at the
 * fill level, the first time the queue is full. From then on the routing tops
 * the queue up to the level with dummy packets before it enqueues a copy of
 * that direction (RegulateStorageQueue), so that the queueing delay stays
 * constant whatever the amount of stored data.
 *
 * Fill period: a ring only reaches its full size at its first switch, which
 * lengthens its cross-plane ISLs by as much as Doppler shortens them until the
 * next switch. Until shortly after the first switch of its ring a storage
 * queue is therefore still filling (IsFilling): gaps that drain it are not
 * topped up with dummies, which would circulate around the ring for good, but
 * filled with new copies, and copies may be stored alone (the routing creates
 * the second copy where the other ring is still filling). Only around a ring
 * switch and at satellites that may not insert copies are gaps topped up, so
 * that they move on to the Doppler backlog or to a satellite that fills them.
 * After the fill period every ring holds as much data as fits right after a
 * switch, and the Doppler backlog between two switches drains at the next.
 *
 * Expired copies: a copy whose time to live has expired is removed by its
 * origin when it passes there; if a new object is waiting, a copy of the new
 * object takes its place in the same stream (ReplaceExpiredOwnCopy). Otherwise
 * the copy is deleted. During the fill period the gap is filled like any
 * other; afterwards the level of the origin is lowered by one, which leaves
 * room for a new copy later. Every copy stored through AdmitObject after the
 * fill period raises the level again, and new copies are only admitted while
 * the level is below the fill level. After the fill period the amount of
 * stored data therefore stays constant: expired copies are replaced by new
 * ones, not by dummies.
 */
class SatelliteForwardingApp : public Application
{
  public:
    static constexpr uint16_t kProtocolData = 0x0800;
    static constexpr uint16_t kProtocolBroadcast = 0x0801;
    static constexpr uint32_t kPayloadSize = 1500; ///< bytes per packet (without header)

    /// Extra queue space above the ISL queue size for every queue.
    static constexpr uint64_t kQueueHeadroom = 200;
    /// Extra queue space of the ring queues (Doppler backlog).
    static constexpr uint64_t kRingQueueHeadroom = 100000;

    /// Copies of a new object that may be stored (see AdmitObject).
    enum class Admission
    {
        NONE,      ///< none: the object has to wait
        UP_ONLY,   ///< UP copy only; the routing creates the DOWN copy later
        DOWN_ONLY, ///< DOWN copy only; the routing creates the UP copy later
        BOTH,
    };

    /// Writes the next waiting copy of `direction` into `header` (identity
    /// fields only); returns false if no copy is waiting.
    using ReplacementSource = std::function<bool(direction_t direction, SatPacketHeader& header)>;

    /// Record of a packet created by this satellite (both copies share it).
    struct PacketRecord
    {
        double created{0.0};      ///< creation time [s]
        double lastSeen{0.0};     ///< last time any copy passed this satellite [s]
        double lastSeenUp{0.0};   ///< last pass of the UP copy (0: none yet) [s]
        double lastSeenDown{0.0}; ///< last pass of the DOWN copy (0: none yet) [s]
    };

    static TypeId GetTypeId();
    SatelliteForwardingApp();
    ~SatelliteForwardingApp() override;

    /// Must be called once before the application starts.
    void Setup(const SatelliteForwardingAppParams& params);

    // ── Output files ─────────────────────────────────────────────────────────
    /// Output folder of all applications (default "mysim_results", relative to
    /// the working directory).
    static void SetOutputDir(const std::string& dir);
    static std::string GetOutputDir();
    /// "<output folder>/<relPath>"; missing parent folders are created.
    static std::string OutputPath(const std::string& relPath);

    // ── Topology and devices ─────────────────────────────────────────────────
    Ptr<NetDevice> GetDevUp() const { return m_devUp; }
    Ptr<NetDevice> GetDevDown() const { return m_devDown; }
    Ptr<NetDevice> GetDevRight() const { return m_devRight; }
    Ptr<NetDevice> GetDevLeft() const { return m_devLeft; }
    uint16_t GetSatId() const { return m_satId; }
    uint32_t GetSatsPerOrbit() const { return m_satsPerOrbit; }
    uint32_t GetNumSatellites() const { return m_numSatellites; }
    uint32_t GetNumOrbits() const { return m_numOrbits; }
    const ConstellationConfig& GetConstellationConfig() const { return m_constellation; }
    Ptr<UniformRandomVariable> GetRandom() const { return m_random; }

    // ── Queues ───────────────────────────────────────────────────────────────
    /// ISL queue size [packets] (without headroom).
    uint64_t GetIslQueueSize() const { return m_islQueueSize; }
    /// Fill level of the storage queues [packets].
    uint64_t GetFillLevel() const { return m_maxQueueFillLevel * m_islQueueSize / 100; }
    /// Number of packets in the data queue of `device`.
    static uint64_t GetQueueOccupancy(Ptr<NetDevice> device);
    /// Gives the ring queues of a new ring member their enlarged limit right
    /// away (AdjustQueueLimits does it on its next tick).
    void GrowRingQueues();
    /// Sends `packet` on `outDev` if the queue of that protocol has space,
    /// otherwise counts it as dropped.
    void ForwardPacket(Ptr<NetDevice> outDev, Ptr<Packet> packet, uint16_t protocol = kProtocolData);
    /// Fills the data queue of `device` with dummy packets up to `level`
    /// packets. Dummies are dropped by the next satellite: they only add
    /// queueing delay. Nothing is sent across the seam.
    void FillWithDummies(Ptr<NetDevice> device, uint64_t level);
    /// Devices on which this satellite sends a new broadcast (chosen by the routing).
    std::vector<Ptr<NetDevice>> GetBroadcastFirstHops() const;

    // ── Storage levels ───────────────────────────────────────────────────────
    bool IsLevelActive(direction_t direction) const;
    /// True while the storage queue of `direction` is filled up to the fill
    /// level with new copies: before its level is active and during the fill
    /// period of its ring (see the class description).
    bool IsFilling(direction_t direction) const;
    /// Called by the routing before it enqueues a copy of `direction` into the
    /// storage queue of `direction`: activates the level once the queue is
    /// full and tops the queue up to the level with dummies, unless the queue
    /// is filling and this satellite can fill it itself.
    void RegulateStorageQueue(direction_t direction);
    /// Decides which copies of a new object of `numPackets` packets may be
    /// stored now and adds them to the levels. Once neither queue is filling,
    /// objects are only stored complete.
    Admission AdmitObject(uint32_t numPackets);
    /// Handles a copy of this satellite whose time to live has expired (not
    /// around a ring switch): a waiting copy of the same direction takes its
    /// place (the header is rewritten and the copy is forwarded as usual),
    /// otherwise the copy is deleted and the level lowered. Returns true if
    /// the copy is deleted, i.e. must not be forwarded.
    bool ReplaceExpiredOwnCopy(SatPacketHeader& header);
    /// Source of the copies that replace expired ones (set by the content strategy).
    void SetReplacementSource(ReplacementSource source) { m_replacementSource = std::move(source); }
    /// A fragment that was stored as a single copy passes this satellite:
    /// stores the missing copy in `direction` with `epoch` if the storage queue
    /// of `direction` is still filling and has room. Returns true if the copy
    /// was created.
    bool CreateSecondCopy(direction_t direction, const SatPacketHeader& header, uint32_t epoch);
    /// True if the time to live of the packet has expired.
    static bool IsExpired(const SatPacketHeader& header);

    // ── Ring roles (set by the routing) ──────────────────────────────────────
    bool IsRingUp() const { return m_ringUp; }
    bool IsRingDown() const { return m_ringDown; }
    void SetRingUp(bool member) { m_ringUp = member; }
    void SetRingDown(bool member) { m_ringDown = member; }
    /// Satellite next to the seam (Walker-Star): its left / right ISL crosses it.
    bool IsSeamLeft() const { return m_seamLeft; }
    bool IsSeamRight() const { return m_seamRight; }
    /// No new copies may be stored in the queue of `direction` (UP / DOWN).
    bool IsInsertionBlocked(direction_t direction) const;
    void SetInsertionBlocked(direction_t direction, bool blocked);
    /// Epoch new copies of `direction` are created with.
    uint32_t GetInsertionEpoch(direction_t direction) const;
    void SetInsertionEpoch(direction_t direction, uint32_t epoch);

    // ── Ring switches ────────────────────────────────────────────────────────
    /// Announces a switch of the UP or DOWN `ring` at `time` [s]
    /// (RingSwitchScheduler, before the simulation starts); used for the switch
    /// guard and the fill period.
    void AddRingSwitchTime(direction_t ring, double time);
    /// Called by RingSwitchScheduler on the satellite that starts a switch.
    void TriggerRingUpSwitch();
    void TriggerRingDownSwitch();
    /// True from shortly before until the queues have settled after a ring
    /// switch: no copies are stored or deleted and no queue is shrunk.
    bool IsInSwitchGuard() const;

    // ── Objects and own packets ──────────────────────────────────────────────
    uint32_t GetObjectSize() const { return m_objectSize; }
    uint64_t GetObjectTtl() const { return m_objectTtl; }
    uint32_t AllocateObjectId() { return m_nextObjectId++; }
    /// Allocates the next packet id together with its record.
    uint32_t AllocatePacketId();
    uint32_t GetNumAllocatedPacketIds() const { return m_nextPacketId; }
    /// Record of packet `id`, nullptr if it has been evicted.
    PacketRecord* GetRecord(uint32_t id);
    /// A copy of one of this satellite's packets passed it: refresh its record
    /// and measure the round-trip time of that copy.
    void RecordReturn(PacketRecord* record, direction_t direction);

    // ── Statistics and event logs ────────────────────────────────────────────
    /// A packet arrived on a port without routing rule (counted as dropped).
    void LogUnroutable(int code, uint32_t lastHop);
    /// mode: 0 both copies, 1 UP copy only, 2 DOWN copy only.
    void LogObjectInsertion(uint32_t objectId, uint32_t numFragments, int mode);
    void LogDuplication(uint16_t origin, uint32_t objectId, uint32_t fragmentId);
    void LogReassembly(uint16_t dupCode, uint16_t origin, uint32_t id);
    void LogBroadcast(bool isSender, uint16_t origin, uint32_t id);

  private:
    /// Level of one storage queue.
    struct StorageLevel
    {
        bool active{false};
        int64_t level{0}; ///< the queue is topped up to this many packets (if > 0)
    };

    void StartApplication() override;

    bool ReceiveFromDevice(Ptr<NetDevice> device,
                           Ptr<const Packet> packet,
                           uint16_t protocol,
                           const Address& sender);

    /// Periodic: ring members get enlarged queues, other queues return to the
    /// normal limit once drained.
    void AdjustQueueLimits();
    Ptr<NetDevice> GetStorageDevice(direction_t direction) const;
    StorageLevel& GetStorageLevel(direction_t direction);
    const StorageLevel& GetStorageLevel(direction_t direction) const;
    /// Activates the level of `direction` once no object fits into its queue.
    void ActivateLevelIfFull(direction_t direction);
    /// True until shortly after the switch guard of the first switch of the
    /// ring of `direction` (always if that ring never switches).
    bool IsInFillPeriod(direction_t direction) const;
    /// Space for new copies in the storage queue of `direction` [packets].
    uint64_t GetStorageSpace(direction_t direction) const;
    void EvictExpiredRecords();
    void WriteQueueStatistics();
    void WritePacketStatistics();

    std::unique_ptr<RoutingStrategy> m_routing;
    std::unique_ptr<ContentStrategy> m_content;
    Ptr<UniformRandomVariable> m_random;

    // topology
    uint16_t m_satId{0};
    uint32_t m_satsPerOrbit{0};
    uint32_t m_numSatellites{0};
    uint32_t m_numOrbits{0};
    ConstellationConfig m_constellation;
    Ptr<NetDevice> m_devUp;
    Ptr<NetDevice> m_devDown;
    Ptr<NetDevice> m_devRight;
    Ptr<NetDevice> m_devLeft;

    // queues
    uint64_t m_islQueueSize{0};
    uint32_t m_maxQueueFillLevel{100};
    StorageLevel m_storageLevels[2]; ///< UP, DOWN
    ReplacementSource m_replacementSource;

    // ring roles
    bool m_ringUp{false};
    bool m_ringDown{false};
    bool m_seamLeft{false};
    bool m_seamRight{false};
    bool m_insertionBlocked[2]{false, false};
    uint32_t m_insertionEpoch[2]{0, 0};
    std::vector<double> m_switchTimes; ///< both rings
    double m_firstSwitchTime[2]{std::numeric_limits<double>::infinity(),
                                std::numeric_limits<double>::infinity()}; ///< UP, DOWN ring [s]

    // objects and own packets
    uint32_t m_objectSize{10};
    uint64_t m_objectTtl{10000};
    uint32_t m_nextObjectId{0};
    uint32_t m_nextPacketId{0};
    std::deque<PacketRecord> m_records; ///< records of ids m_firstRecordId, m_firstRecordId + 1, ...
    uint32_t m_firstRecordId{0};

    // statistics
    uint32_t m_runNumber{0};
    uint32_t m_numDropped{0};
    uint64_t m_numStaleRecords{0}; ///< copies that arrived after their record was evicted
    double m_rttSum{0.0};          ///< round-trip times measured since the last statistics line
    uint64_t m_rttCount{0};
    double m_meanRtt{10.0}; ///< mean round-trip time of the last interval (10.0: none measured yet)
    double m_maxRtt{0.0};   ///< longest round-trip time measured so far

    // output files
    std::string m_queueStatsFile;
    std::string m_packetStatsFile;
    std::string m_reassemblyFile;
    std::string m_broadcastFile;
    std::string m_insertionFile;
    std::string m_duplicationFile;
    std::string m_unroutableFile;
};

} // namespace ns3

#endif // SATELLITE_FORWARDING_APP_H
