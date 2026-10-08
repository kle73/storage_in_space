#ifndef ROUTING_RING_SWITCH_STAR_H
#define ROUTING_RING_SWITCH_STAR_H

#include "../strategy-interfaces.h"
#include <unordered_set>
#include <cstdint>
#include "ns3/header.h"

namespace ns3 {

// Header carried by every storage packet (UP / DOWN / DUMMY / BROADCAST).
//   id        packet id (unique per origin satellite)
//   sat       origin satellite (the one that keeps the PacketRecord)
//   time      creation time [ms]           ttl   time to live [s]
//   direction UP / DOWN / DUMMY / BROADCAST (direction_t)
//   epoch     ring-switch epoch of the ring direction the packet travels in;
//             stamped by every ring satellite that forwards it (switch flag)
//   obj_id / frag_id  object and fragment number
//   last_hop  satellite that sent it (debugging)
//   dup_code  > 0: the second copy of this object still has to be created
//             (RoutingRingSwitchStar::DuplicatePacket)
class RingSwitchStarHeader : public Header
{
public:
    static TypeId GetTypeId (void);
    TypeId GetInstanceTypeId (void) const override;

    void Serialize   (Buffer::Iterator start) const override;
    uint32_t Deserialize (Buffer::Iterator start) override;
    void Print       (std::ostream& os) const override;
    uint32_t GetSerializedSize (void) const override { return 42; }

    void     SetId          (uint32_t id)   { m_id      = id; }
    void     SetSat         (uint16_t s)    { m_sat     = s;  }
    void     SetTime        (uint64_t t)    { m_time    = t;  }
    void     SetTTL         (uint64_t ttl)  { m_ttl     = ttl;  }
    void     SetDirection   (uint16_t d)    { m_direction = d;  }
    void     SetEpoch       (uint32_t e)    { m_epoch     = e; }
    void     SetObjId       (uint32_t oid)  { m_obj_id  = oid; }
    void     SetFragId      (uint32_t fid)  { m_frag_id = fid; }
    void     SetLastHop     (uint32_t lh)   { m_last_hop = lh; }
    void     SetDupCode     (uint16_t dc)   { m_dup_code = dc; }

    uint32_t GetId       (void) const  { return m_id;      }
    uint16_t GetSat      (void) const  { return m_sat;     }
    uint64_t GetTime     (void) const  { return m_time;    }
    uint64_t GetTTL      (void) const  { return m_ttl;    }
    uint16_t GetDirection(void) const  { return m_direction;}
    uint32_t GetEpoch    (void) const  { return m_epoch; }
    uint32_t GetObjId    (void) const  { return m_obj_id;  }
    uint32_t GetFragId   (void) const  { return m_frag_id; }
    uint32_t GetLastHop  (void) const  { return m_last_hop; }
    uint16_t GetDupCode  (void)  const  { return m_dup_code; } 

private:
    uint32_t m_id       {0};
    uint16_t m_sat      {0};
    uint64_t m_time     {0};
    uint64_t m_ttl      {0};
    uint16_t m_direction{0};
    uint32_t m_epoch    {0};

    uint32_t m_obj_id   {0};
    uint32_t m_frag_id  {0};

    uint32_t m_last_hop {0};
    uint16_t m_dup_code   {0};

};

/**
 * Ring routing with periodic ring relocation for Walker-Star constellations
 * (writeup "Storage in Space", sections 2.2 and 2.4).
 *
 * Every object circulates as an UP copy (travels upwards inside an orbit) and a
 * DOWN copy (travels downwards). A copy goes once around its orbit (one lap)
 * and then changes orbit at the ring satellite of that orbit; at the seam the
 * copies return along the ring row. Cross-seam ISLs are never used.
 *
 * Ring switch: every ~548 s (Iridium) the RingSwitchScheduler calls
 * InitiateRingUpSwitch() / InitiateRingDownSwitch() on the new seam-right
 * satellite (one position below the current ring). The switch spreads with the
 * data traffic itself, using an epoch number in the packet header as flag:
 * satellites of the new row activate when the new epoch arrives on devRight,
 * satellites of the old row retire when it arrives along the orbit.
 *
 * Full description, including where copies take a shortcut during a switch
 * ([UP-SHORTCUT], [DOWN-LAP-SKIP]) and the queue-preserving dummy packets
 * ([DUMMIES], [DOWN-REGULATION]): see the overview at the top of
 * routing-ring-switch-walker-star.cc.
 */
class RoutingRingSwitchStar: public RoutingStrategy
{
public:
    bool OnReceive (Ptr<NetDevice>    device,
                    Ptr<const Packet> packet,
                    uint16_t          protocol,
                    const Address&    sender) override;

    void Init(SatelliteForwardingApp *app) override;

    // Called by the RingSwitchScheduler (via SatelliteForwardingApp::TriggerRing*)
    // on the NEW seam-right satellite at the scheduled switch time.
    void InitiateRingUpSwitch   ();
    void InitiateRingDownSwitch ();

private:
    enum class RingRole { NONE, NORMAL, SEAM_LEFT, SEAM_RIGHT };

    // Current role of this satellite in the UP / DOWN ring: NONE (not ring),
    // NORMAL (orbits 0-3), SEAM_LEFT (orbit 5) or SEAM_RIGHT (orbit 4).
    RingRole ComputeUpRole()   const;
    RingRole ComputeDownRole() const;

    char filename_packet_monitoring[512];

    // Highest DOWN / UP switch epoch this satellite has seen (on passing traffic
    // or as initiator). Activation and retirement only react to packets with a
    // strictly greater epoch; the next initiator uses this value + 1.
    // (m_downEpoch / m_upEpoch are not used.)
    uint32_t m_downEpoch         {0};
    uint32_t m_maxDownEpochSeen  {0};

    // [DOWN-REGULATION] (.cc overview 6): steady DOWN queue level of this
    // satellite = occupancy of its DOWN queue at the first DOWN packet after
    // STORAGE_FILL_PHASE_END_S. Regulation is active once it is recorded.
    uint64_t m_downBaseOcc         {0};
    bool     m_downBaseRecorded    {false};
    
    uint32_t m_upEpoch         {0};
    uint32_t m_maxUpEpochSeen{0};

    // ── Broadcast State ───────────────────────────────────────────────────────
    // Duplicate suppression bounded by a two-window rotation: entries survive
    // between 1x and 2x TTL, never less, so a packet that could still legally
    // arrive is always still recorded.
    // std::unordered_set<uint64_t> m_seenCurr;
    // std::unordered_set<uint64_t> m_seenPrev;
    // uint64_t m_seenWindowStart {0};
    // bool SeenBefore (uint64_t key, uint64_t now_ms);

    static constexpr uint32_t SEEN_W     = 32768;      // ids per origin in the window
    static constexpr uint32_t SEEN_WORDS = SEEN_W / 64;

    struct SeenWindow {
        uint32_t base {0};              // lowest id represented; always a multiple of 64
        bool     init {false};
        std::vector<uint64_t> bits;
    };
    std::vector<SeenWindow> m_seen;     // indexed by origin sat id
    uint64_t m_seen_too_old {0};        // canary: must stay ~0

    bool SeenBefore (uint16_t origin, uint32_t id);

    // ── Helpers ───────────────────────────────────────────────────────────────
    // fill the queue of `device` with dummy packets up to amount-1 packets
    void InsertDummyPackets(uint32_t amount, Ptr<NetDevice> device);
    // Forward a DOWN packet on `outDev`. [DOWN-REGULATION]: first top the
    // target queue up to its steady level with dummies (DOWN queue: own steady
    // DOWN level; left queue of a NORMAL DOWN ring satellite: ISL queue size).
    // `role` = this satellite's DOWN ring role (NONE for normal forwarding).
    void ForwardDown(Ptr<NetDevice> outDev, Ptr<Packet> pkt, RingRole role);

    // Down switch (epoch-based, Algorithm 1)
    void ActivateNewDownRing(uint32_t epoch);
    void RetireDownRing(RingRole actingRole, uint32_t epoch);

    // Up switch (epoch-based)
    void ActivateNewUpRing(uint32_t epoch);
    void RetireUpRing(RingRole actingRole, uint32_t epoch);

    // second copy of a flagged object in `direction` (fill phase only)
    bool DuplicatePacket(direction_t direction, uint32_t id, uint16_t origin, 
                        uint64_t ttl, uint32_t obj_id, uint32_t frag_id, uint64_t time);

    // Routing tables (`device` = device the packet arrived on)
    void RouteUpRing  (RingRole role, Ptr<NetDevice> device, Ptr<Packet> pkt, uint32_t last_hop);
    void RouteDownRing(RingRole role, Ptr<NetDevice> device, Ptr<Packet> pkt, uint32_t last_hop);
    void RouteNormalUp  (Ptr<NetDevice> device, Ptr<Packet> pkt, uint32_t last_hop);
    void RouteNormalDown (Ptr<NetDevice> device, Ptr<Packet> pkt, uint32_t last_hop);
    void LogUnknownDevice(int code, uint32_t last_hop);

};

} // namespace ns3
#endif // ROUTING_RING_SWITCH_STAR_H
