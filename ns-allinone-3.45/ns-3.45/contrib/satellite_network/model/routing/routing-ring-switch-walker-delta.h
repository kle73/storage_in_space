#ifndef ROUTING_RING_SWITCH_DELTA_H
#define ROUTING_RING_SWITCH_DELTA_H

#include "../strategy-interfaces.h"
#include <unordered_set>
#include <cstdint>
#include "ns3/header.h"

namespace ns3 {

// Routing Switxh Header Definition
class RingSwitchDeltaHeader : public Header
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
 * Ring routing with periodic ring relocation for Walker-Delta constellations
 * (e.g. Starlink). No seam: both rings are closed loops around the globe.
 *
 * Every copy goes once around its orbit (UP copies upwards, DOWN copies
 * downwards) and leaves the orbit at the orbit's EXIT satellite: UP exits
 * forward to the right, DOWN exits forward to the left. The exits of all
 * orbits form the ring row. Because the inter-orbit ISLs of the last and the
 * first orbit are shifted by one position (Walker phasing), one orbit per ring
 * has its entry one position away from its exit (kink orbit).
 *
 * Ring switch (RingSwitchScheduler -> InitiateRingUpSwitch() /
 * InitiateRingDownSwitch()), all exits move down by one position per switch:
 *   UP  : role wave started by the new exit of the UP kink orbit; a satellite
 *         takes its role for a new epoch when it first sees it.
 *   DOWN: per-copy routing, every DOWN copy exits at the exit of the epoch it
 *         carries; the switch is started by the new exit of the DOWN kink
 *         orbit, which gives the new epoch to the copies entering there.
 * Full description: overview at the top of routing-ring-switch-walker-delta.cc.
 */
class RoutingRingSwitchDelta: public RoutingStrategy
{
public:
    bool OnReceive (Ptr<NetDevice>    device,
                    Ptr<const Packet> packet,
                    uint16_t          protocol,
                    const Address&    sender) override;

    void Init(SatelliteForwardingApp *app) override;

    // Made PUBLIC so RingSwitchScheduler (via SatelliteForwardingApp::TriggerRing*) can call them.
    void InitiateRingUpSwitch   ();
    void InitiateRingDownSwitch ();

private:
    static constexpr uint16_t BROADCAST_PROTO = 0x0801;

    char filename_packet_monitoring[512];

    // Highest UP / DOWN switch epoch this satellite has seen (on passing
    // traffic or as initiator). The initiator of the next switch uses +1.
    uint32_t m_maxDownEpochSeen  {0};
    uint32_t m_maxUpEpochSeen    {0};

    // ── Ring roles (overview 2-5 in the .cc) ─────────────────────────────────
    // Exit position of every orbit at epoch 0 (from the constellation config,
    // one ring satellite per orbit); at epoch e the exit of orbit k is
    // (exit0[k] - e) mod No.
    std::vector<int32_t> m_upExit0;
    std::vector<int32_t> m_downExit0;
    bool     m_rolesInit {false};
    bool     m_upExit    {false};     // UP exit of epoch m_maxUpEpochSeen (exits UP lap traffic to the right)
    bool     m_downExit  {false};     // DOWN exit of epoch m_maxDownEpochSeen (ring membership only,
                                      // DOWN copies are routed by their own epoch)
    uint32_t m_leftPeer  {UINT32_MAX};// sat id of the left / right ISL neighbour
    uint32_t m_rightPeer {UINT32_MAX};
    // DOWN switch (overview 5): epoch given to copies entering over devRight
    // (non-zero only on DOWN switch initiators) and the newest epoch this
    // satellite has exited (dummies on the first exit of a new epoch).
    uint32_t m_downInitiated {0};
    uint32_t m_downExitEpoch {0};
    // DOWN queue regulation (overview 6): DOWN queue level at the end of the
    // fill phase, restored with dummies before a DOWN copy is queued.
    bool     m_downBaseRecorded {false};
    uint64_t m_downBaseOcc      {0};
    void RegulateDownQueue ();

    void InitRoles ();
    bool IsUpExitSat   (uint32_t sat, uint32_t epoch) const;
    bool IsDownExitSat (uint32_t sat, uint32_t epoch) const;
    void UpdateUpRole   (uint32_t epoch, bool announce, bool initiator);
    void UpdateDownRole (uint32_t epoch);

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

    // second copy of a flagged object in `direction` (fill phase only)
    bool DuplicatePacket(direction_t direction, uint32_t id, uint16_t origin, 
                        uint64_t ttl, uint32_t obj_id, uint32_t frag_id, uint64_t time);

    // Output device for an UP / DOWN copy that arrived on `device`
    // (nullptr: no rule -> counted as dropped). `crossesRing` is set when the
    // copy leaves the orbit over the ring ISL. UP: the copy then carries the
    // exit's epoch. DOWN: the copy's own `epoch` decides where it exits; the
    // switch initiator raises it for copies entering there.
    Ptr<NetDevice> RouteUp   (Ptr<NetDevice> device, bool& crossesRing) const;
    Ptr<NetDevice> RouteDown (Ptr<NetDevice> device, uint32_t& epoch, bool& crossesRing);
    void LogUnknownDevice(int code, uint32_t last_hop);

};

} // namespace ns3
#endif // ROUTING_RING_SWITCH_DELTA_H
