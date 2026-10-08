// =============================================================================
//  routing-ring-switch-walker-star.cc
//
//  Ring routing for distributed storage in a Walker-Star constellation
//  (writeup "Storage in Space", sections 2.2 and 2.4), including the periodic
//  relocation of both rings ("ring switch").
//
//  Quick index (search for these tags):
//    [TOPOLOGY]          naming, directions, seam                   (overview 1)
//    [RING-RULES]        steady-state forwarding of both rings      (overview 2)
//    [UP-SHORTCUT]       UP switch: every UP copy skips ONE hop once  (overview 4)
//    [DOWN-LAP-SKIP]     DOWN switch: every DOWN copy leaves ONE orbit
//                        after 1 hop instead of a full lap, once     (overview 5)
//    [SEAM-FLUSH]        why the seam-right orbit drains at a DOWN switch
//    [DOWN-REGULATION]   fix: DOWN queues and DOWN cross links are kept at
//                        their steady level with dummy packets (refills the
//                        drained seam orbit, gives the initial ring its
//                        cross-link backlog, stops residual deviations from
//                        being carried from switch to switch)   (overview 6)
//    [LATE-TRAFFIC]      retired ring satellites, never use a seam link
//    [DUMMIES]           every place dummy packets are inserted
// =============================================================================
#include "routing-ring-switch-walker-star.h"
#include "../ring-switch-scheduler.h"
#include "../satellite-forwarding-app.h"

#include "ns3/simulator.h"
#include "ns3/mobility-model.h"
#include "ns3/queue.h"
#include "ns3/point-to-point-laser-net-device.h"
#include "ns3/node.h"

#include <cmath>

namespace ns3 {

// ── Configuration of the DOWN queue regulation (overview 6) ──────────────────
// End of the fill phase. Must be equal to STORAGE_FILL_PHASE_END_S in
// content/content-fill-double.h (that header is not included here).
#ifndef STORAGE_FILL_PHASE_END_S
#define STORAGE_FILL_PHASE_END_S 20.0
#endif
// 1 = [DOWN-REGULATION] before a satellite puts a DOWN packet into a DOWN
//     queue, it tops that queue up to the satellite's steady DOWN level with
//     dummy packets; a DOWN ring satellite in orbits 0-3 tops its left queue
//     (forward cross link) up to the ISL queue size. Active from
//     STORAGE_FILL_PHASE_END_S on (the steady level is recorded then).
//     0 = original behaviour (orbit 4 drains at every DOWN switch, lumps).
#define DOWN_QUEUE_REGULATION 1
// Top-ups of at least this many dummies are printed to the console (the
// refill of the drained seam orbit, the initial cross-link bars); smaller
// top-ups are silent.
#define DOWN_REGULATION_LOG_MIN 100

// =============================================================================
// OVERVIEW
// =============================================================================
//
// 1. [TOPOLOGY] NAMING AND DIRECTIONS (code frame; Iridium: O = 6 orbits of
//    No = 11 satellites, sat id = orbit * 11 + position)
//
//    devUp   : next satellite in the same orbit   (position + 1)
//    devDown : previous satellite in the orbit    (position - 1)
//    devLeft / devRight : satellite of the neighbouring orbit along the
//              inter-orbit path. From left to right the orbits are
//                     5 | 0  1  2  3  4 |
//              The seam lies between orbit 4 and orbit 5 (counter-rotating
//              planes). isSeamLeft is set on every satellite of orbit 5
//              (55-65), isSeamRight on every satellite of orbit 4 (44-54).
//              The cross-seam ISLs (devLeft of orbit 5, devRight of orbit 4)
//              are NEVER used for storage traffic.
//
//    Every object is stored as two copies: an UP copy that travels upwards
//    (devUp) inside an orbit and a DOWN copy that travels downwards (devDown).
//    Each direction has its own ring: one satellite per orbit, all at the same
//    position ("ring row"), where copies change orbit.
//
// 2. [RING-RULES] STEADY STATE (RouteUpRing / RouteDownRing / RouteNormal*)
//
//    UP ring, forward direction = to the right (5 -> 0 -> 1 -> 2 -> 3 -> 4):
//      ring satellite (NORMAL)
//        from devLeft  -> devUp     copy enters this orbit
//        from devDown  -> devRight  copy has done its lap, goes to the next orbit
//        from devRight -> devLeft   return stream passes through
//      seam-right ring satellite (orbit 4, end of the forward direction)
//        from devDown  -> devLeft   lap done: start of the return stream
//        from devLeft  -> devUp     copy enters orbit 4
//      seam-left ring satellite (orbit 5, start of the forward direction)
//        from devRight -> devUp     return stream ends: copy enters orbit 5
//        from devDown  -> devRight  lap done, forward to orbit 0
//
//    DOWN ring, forward direction = to the left (4 -> 3 -> 2 -> 1 -> 0 -> 5):
//      ring satellite (NORMAL)
//        from devRight -> devDown   copy enters this orbit
//        from devUp    -> devLeft   copy has done its lap, goes to the next orbit
//        from devLeft  -> devRight  return stream passes through
//      seam-left ring satellite (orbit 5, end of the forward direction)
//        from devUp    -> devRight  lap done: start of the return stream
//        from devRight -> devDown   copy enters orbit 5
//      seam-right ring satellite (orbit 4, start of the forward direction)
//        from devLeft  -> devDown   return stream ends: copy enters orbit 4
//        from devUp    -> devLeft   lap done, forward to orbit 3
//
//    All other satellites only pass copies on in their direction
//    (RouteNormalUp / RouteNormalDown). A full LAP = No = 11 in-orbit hops:
//    a copy enters an orbit at the ring satellite, goes once around the orbit
//    and leaves at the ring satellite again.
//
// 3. WHY AND HOW THE RINGS MOVE
//
//    Cross-orbit ISLs cannot be used close to the poles, so each ring row has
//    to stay in its latitude band while the satellites move on. Every ~548 s
//    (Iridium) both rings therefore move from position n to position n-1
//    (RingSwitchScheduler, condition (6): at switch time every cross link of
//    the new row is longer than the corresponding link of the old row).
//      UP ring  : position n-1 is one hop BEFORE n in the UP travel direction
//                 (the ring moves upstream).
//      DOWN ring: position n-1 is one hop AFTER  n in the DOWN travel direction
//                 (the ring moves downstream).
//    The writeup numbers the DOWN case mirrored (orbit 0 = start of the
//    forward direction, new ring at n+1); code orbit 4 = writeup orbit 0.
//
//    Switch flag = EPOCH NUMBER (header field epoch, m_maxUp/DownEpochSeen).
//    Every satellite passively learns the newest epoch from the traffic passing
//    through it. Activation and retirement only react to a packet whose epoch
//    is STRICTLY GREATER than the newest epoch the satellite has seen, so flags
//    of earlier switch cycles can never trigger a later cycle. Every packet
//    forwarded by ring rules is stamped with the ring satellite's epoch.
//    Both switches are carried by the data traffic itself (no control packet).
//
// 4. UP SWITCH (writeup 2.4.1)
//
//    Initiate   : the scheduler calls InitiateRingUpSwitch() on the new
//                 seam-right m(4) (position n-1 in orbit 4). It becomes ring
//                 with epoch maxSeen+1 and sends the lap traffic it receives
//                 from below to the left (new return row).
//    Activation : a satellite that receives a newer epoch on devRight (the new
//                 return row) becomes ring (ActivateNewUpRing), inserts dummy
//                 packets on its devRight [DUMMIES] and forwards by ring rules.
//                 The wave runs leftwards along the new row up to orbit 5.
//    Retirement : an old ring satellite n(p) retires when a newer epoch arrives
//                 from below (devDown): a copy that entered orbit p at m(p) one
//                 hop below. From then on it forwards by normal rules.
//    [UP-SHORTCUT] Once m(p) is active, the lap traffic coming up the orbit
//                 reaches m(p) one hop BEFORE the old ring satellite n(p) and
//                 leaves the orbit there: every UP copy in flight does 10
//                 instead of 11 hops in one orbit, exactly once per UP switch
//                 (mini-sim: all ~3.6M UP copies, none twice). Nothing is
//                 skipped beyond this single hop.
//
// 5. DOWN SWITCH (writeup 2.4.2, Algorithm 1)
//
//    Initiate   : the scheduler calls InitiateRingDownSwitch() on the new
//                 seam-right m(4) (position n-1 in orbit 4, one hop BELOW the
//                 old seam-right n(4)). It becomes ring with epoch maxSeen+1
//                 and from now on stamps everything it forwards by ring rules
//                 (equivalent to the writeup's s0,n flagging everything it
//                 sends to s0,n+1).
//    Activation : a satellite that receives a newer epoch on devRight (the new
//                 forward row) becomes ring (ActivateNewDownRing), inserts
//                 dummy packets on its devLeft [DUMMIES] and forwards by ring
//                 rules (Alg. 1 l. 6-11). The wave runs leftwards along the new
//                 row up to the new seam-left in orbit 5.
//    Retirement : an old ring satellite n(p) retires when a newer epoch arrives
//                 from above (devUp): a copy that entered orbit p at m(p) and
//                 has gone around the orbit (10 hops). n(p) then passes it on
//                 downwards to m(p) (11th hop), where it leaves the orbit
//                 (Alg. 1 l. 17-21). The old seam-right n(4) retires last.
//
//    [DOWN-LAP-SKIP] Because the new ring satellite m(p) is one hop AFTER the
//    old one, every copy that enters orbit p at the OLD ring satellite n(p)
//    while m(p) is already active reaches m(p) after one hop, arrives "from
//    above" and is sent to the next orbit by the ring rule
//    (devUp -> devLeft / devRight). It leaves orbit p after 1 hop instead of
//    11. This happens for
//      - orbit 4: the old return stream that n(4) still puts into orbit 4
//        until the old return stream ends (~0.3 s, InitiateRingDownSwitch,
//        RouteDownRing SEAM_RIGHT);
//      - every other orbit p: the copies that completed their lap in orbit p+1
//        on the old ring and enter orbit p via the old row (RouteDownRing
//        NORMAL / SEAM_LEFT, RouteNormalDown after n(p) has retired).
//    Result: every DOWN copy in flight leaves ONE orbit after 1 hop instead of
//    a full lap, exactly once per DOWN switch; afterwards all copies do full
//    laps in all orbits again. Mini-sim (switch at 300 s): ~1.0M copies did
//    this in orbit 4, ~0.5M in each other orbit, none twice, none lost.
//    This is part of Algorithm 1 as written (writeup: "From this point on, no
//    packets are sent to s0,n+2 until flagged packets return from the new right
//    seam"; t_p0 = d(s0,n+1, s1,n+1) + T_orb(1, n+1), i.e. p0 does not pass
//    through orbit 0). It is the price of moving a ring downstream: leaving one
//    hop LATE instead would delay those copies by one hop and make them collide
//    on the link n(p) -> m(p) with the traffic that still enters at n(p);
//    leaving after 1 hop (= one hop late minus a full lap) is collision-free.
//    The UP ring moves upstream and gets away with the one-hop [UP-SHORTCUT].
//    Side effect: a copy whose origin satellite lies in the skipped part of the
//    orbit passes its origin once less (Avg RTT steps up at DOWN switches).
//
//    [SEAM-FLUSH] Orbit 4 only receives input through the seam-right ring
//    satellite (devLeft -> devDown). From the initiation until the new return
//    stream reaches m(4) (~140 ms in the Iridium run) the old return stream is
//    skipped out of orbit 4 at m(4) and nothing else enters, so ALL DOWN queues
//    of orbit 4 drain. Without [DOWN-REGULATION] this broke the seam condition
//    (9)/(10) (see overview 6b, 6d).
//
// 6. QUEUEING DELAY, DUMMY PACKETS AND PERIODICITY
//
//    The timing conditions (5), (7)/(8), (9)/(10) of the writeup compare the
//    propagation delay of two paths. With queues they hold only if both paths
//    also have the same queueing delay; the writeup's remedy is to insert
//    dummy packets ("deleted at the next hop") into the emptier path. A dummy
//    backlog in front of a stream behaves like a queue: real packets that keep
//    arriving at line rate queue up behind the dummies, so after the dummies
//    are gone the queue holds the same number of REAL packets.
//
//    6a [DUMMIES] cross links at activation:
//       UP  : a new ring satellite puts as many dummies on its new devRight as
//             its own up queue holds (= queue of the old path).
//       DOWN: a new ring satellite fills its new devLeft up to the ISL queue
//             size (original rule; this is also the level [DOWN-REGULATION]
//             keeps the DOWN cross links at).
//       The standing backlog behind the dummies stays for the whole ring
//       period (the steady bars in the right/left queue plots) and gives the
//       next switch an old cross link with the same queue as the new one.
//
//    6b Seam condition (9)/(10): the flagged packets reach the old seam-right
//       n(4) only after passing orbit 4, which [SEAM-FLUSH] has drained.
//       Without queues in orbit 4 they were (No-1) queue delays (~12 ms at
//       ~4000 packets per queue) faster than the old traffic the condition
//       compares them with: n(4) retired ~9 ms too early, the old return
//       stream that was still arriving merged into its DOWN queue (~40k packet
//       lump), orbit 4 stayed empty once admission was frozen, and each later
//       switch moved the lump on and turned part of it into knock-on lumps in
//       the inner orbits.
//
//    6c Inner orbits, condition (7)/(8): compares the pass through orbit p
//       with the pass through orbit p+1, and the new cross link with the old
//       one. The ring geometry itself is periodic: the Doppler backlog an old
//       ring satellite collects during its period is exactly what the switch
//       gives back (the new row is longer). The switch is therefore periodic
//       only if all orbits hold the same queue content. The UP conditions
//       compare an orbit with ITSELF; the DOWN conditions compare DIFFERENT
//       orbits. Any difference between orbits (startup, a lump, rounding)
//       makes a DOWN switch end a little early or late in some orbit, which
//       moves packets between the queues of the orbits involved. The next
//       switch compares other queues (the ring has moved on), so without
//       regulation these residual deviations are carried from switch to
//       switch and grow when the ring passes the same positions again (in the
//       mini-sim visibly from the second orbit period, ~6000 s, on).
//
//    6d [DOWN-REGULATION] (DOWN_QUEUE_REGULATION, ForwardDown): from the end
//       of the fill phase on, a satellite that puts a DOWN packet
//         - into its DOWN queue first tops that queue up with dummies to its
//           steady DOWN level (its own level at the end of the fill phase,
//           at most the ISL queue size);
//         - into its left queue (forward cross link) as a NORMAL DOWN ring
//           satellite (orbits 0-3) first tops that queue up to the ISL queue
//           size (the size of the activation bar, 6a).
//       Queues ABOVE the level (Doppler ramps, backlogs) are not touched; only
//       a deficit is filled, and only with dummies, so no stored copy is
//       created or lost (dummies are dropped at the next hop). Effects:
//         - seam-right orbit: when the new return stream reaches m(4) and the
//           first flagged packets pass through orbit 4, every drained DOWN
//           queue is restored just before the packet enters it. The flagged
//           packets see the same queueing delay as the old traffic, (9)/(10)
//           holds, n(4) retires after the old return stream has ended and
//           orbit 4 keeps its data (console: "DOWN queue topped up").
//         - (7)/(8): all DOWN paths that are compared have the same queue
//           level in every orbit, so a switch has no deviation to carry over;
//           residual deviations are removed instead of passed on.
//         - the initial DOWN ring, which never went through an activation,
//           gets its cross-link bars at the end of the fill phase.
//       Normal operation is not affected: every queue at or above its level
//       gets no dummies. The UP ring needs no regulation (6c).
//
//    6e First switch: the initial ring has run for less than a full period
//       when the first DOWN switch happens (less Doppler backlog than the
//       switch gives back), so that switch over-drains a few DOWN queues once.
//       The storage budget is therefore frozen only after the first DOWN
//       switch; admission refills those queues with real data before
//       (FREEZE_AFTER_FIRST_DOWN_SWITCH in content-fill-double.h).
//
// 7. [LATE-TRAFFIC] RETIRED RING SATELLITES (RouteNormalUp / RouteNormalDown)
//    A satellite that is no longer ring can still receive copies on its ring
//    ports for a short time. It handles them like the ring would have (late
//    forward traffic enters the orbit, late return traffic continues along the
//    old row and enters the orbit at the old terminus) and NEVER uses a seam
//    link (that created packets circling forever through both seam links).
//    Every packet that cannot be forwarded is counted as dropped.
//    Queue limits of ring / retired satellites: see
//    SatelliteForwardingApp::AdjustQueueBuffer.
//
// 8. WHAT THE QUEUE PLOTS SHOULD SHOW WITH THIS VERSION
//    - Doppler ramps: the queue right behind a shrinking cross link of the
//      current ring grows during the ring period (UP: ring satellite up queue,
//      left queue of the return path and up queue of the seam-left ring
//      satellite; DOWN: ring satellite down queue, right queue of the return
//      path and down queue of the seam-right ring satellite) and is given back
//      at the next switch.
//    - Dummy bars: right queues of the UP ring (not orbit 4: seam link), left
//      queues of the DOWN ring in orbits 0-3 (orbit 5: seam link; orbit 4: the
//      initiator inserts none), one bar per orbit and ring period; the DOWN
//      bars from the end of the fill phase on ([DOWN-REGULATION]).
//    - DOWN queues: no drained orbit 4 and no lumps after a DOWN switch; the
//      queues of orbit 4 are topped up ~0.1-0.2 s after the switch.
//    - UP: the up queue of a new UP ring satellite starts empty and ramps up
//      (its input is the new forward stream, which starts behind the dummies of
//      the previous orbit); it is back at the normal level after retirement.
//    - All other queues stay at their fill level.
// =============================================================================

NS_OBJECT_ENSURE_REGISTERED (RingSwitchStarHeader);

TypeId RingSwitchStarHeader::GetTypeId (void)
{
    static TypeId tid = TypeId ("ns3::RingSwitchStarHeader")
        .SetParent<Header> ()
        .SetGroupName ("SatelliteNetwork")
        .AddConstructor<RingSwitchStarHeader> ();
    return tid;
}

TypeId RingSwitchStarHeader::GetInstanceTypeId (void) const { return GetTypeId (); }

// Wire format (42 bytes): id(4) sat(2) time(8) ttl(8) direction(2) epoch(4)
// obj_id(4) frag_id(4) last_hop(4) dup_code(2)
void RingSwitchStarHeader::Serialize (Buffer::Iterator start) const
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

uint32_t RingSwitchStarHeader::Deserialize (Buffer::Iterator start)
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

void RingSwitchStarHeader::Print (std::ostream& os) const
{
    os << "RingSwitchStarHeader dir=" << m_direction
       << " id=" << m_id
       << " time=" << m_time
       << " sat=" << m_sat;
}

// bool
// RoutingRingSwitchStar::SeenBefore (uint64_t key, uint64_t now_ms)
// {
    // if (now_ms - m_seenWindowStart >= (uint64_t) 4)
    // {
        // m_seenPrev.swap (m_seenCurr);
        // m_seenCurr.clear ();
        // m_seenWindowStart = now_ms;
    // }
    // if (m_seenCurr.count (key) || m_seenPrev.count (key)) return true;
    // m_seenCurr.insert (key);
    // return false;
// }

// Broadcast duplicate suppression: per origin a sliding bit window of SEEN_W
// ids. Returns true if (origin, id) was seen before (or is older than the
// window, i.e. TTL-expired anyway), otherwise marks it as seen.
bool
RoutingRingSwitchStar::SeenBefore (uint16_t origin, uint32_t id)
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
void RoutingRingSwitchStar::Init(SatelliteForwardingApp *app)
{
    m_app = app;

    // <output folder>/packet_stats/packet_monitoring_dataFix.csv
    SatelliteForwardingApp::OutputPath ("packet_stats/packet_monitoring_dataFix.csv",
                                        filename_packet_monitoring, sizeof (filename_packet_monitoring));
    if (m_app->GetSatId() == 0) {
        FILE* f = fopen(filename_packet_monitoring, "w");
        if (f) { fprintf(f, "node,time,packet_id,n,p,cameLeft\n"); fclose(f); }
    }
    if (m_app->isSeamLeft){
        printf("left: %d\n", m_app->GetSatId());
    }
    if (m_app->isSeamRight){
        printf("right: %d\n", m_app->GetSatId());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Effective role: NONE (not ring), NORMAL ring satellite, or ring satellite in
// the seam-left / seam-right orbit. isRingUp / isRingDown are set by the
// initial configuration and changed only by Initiate / Activate / Retire.
// ─────────────────────────────────────────────────────────────────────────────
RoutingRingSwitchStar::RingRole RoutingRingSwitchStar::ComputeUpRole() const
{
    if (m_app->isRingUp) {
        if (m_app->isSeamLeft)  return RingRole::SEAM_LEFT;
        if (m_app->isSeamRight) return RingRole::SEAM_RIGHT;
        return RingRole::NORMAL;
    }
    return RingRole::NONE;
}

RoutingRingSwitchStar::RingRole RoutingRingSwitchStar::ComputeDownRole() const
{
    if (m_app->isRingDown) {
        if (m_app->isSeamLeft)  return RingRole::SEAM_LEFT;
        if (m_app->isSeamRight) return RingRole::SEAM_RIGHT;
        return RingRole::NORMAL;
    }
    return RingRole::NONE;
}

// ─────────────────────────────────────────────────────────────────────────────
// [DUMMIES] Dummy packet helper
//   Fills the queue of `device` with DUMMY packets until it holds amount-1
//   packets (nothing is inserted if it already holds at least that many).
//   Dummies are dropped by the receiving satellite (OnReceive), i.e. they only
//   occupy this one queue/link: they delay everything queued behind them by
//   (number of dummies) x (packet service time). Real packets that keep
//   arriving meanwhile queue up behind them, so the queue keeps the same
//   number of real packets afterwards.
//   Never sends over a cross-seam ISL.
// ─────────────────────────────────────────────────────────────────────────────
void RoutingRingSwitchStar::InsertDummyPackets(uint32_t amount, Ptr<NetDevice> device)
{
    // Never put anything on a cross-seam ISL (e.g. the new DOWN seam-left
    // would otherwise send its dummies over the seam).
    if ((device == m_app->GetDevLeft()  && m_app->isSeamLeft) ||
        (device == m_app->GetDevRight() && m_app->isSeamRight))
        return;

    Ptr<Queue<Packet>> q = DynamicCast<PointToPointLaserNetDevice>(device)->GetQueue();
    uint64_t qSize = q->GetCurrentSize().GetValue();
    while (amount > qSize+1)
    {
        Ptr<Packet> pkt_new = Create<Packet>(SatelliteForwardingApp::maxPayloadSize);
        RingSwitchStarHeader hdr_new;
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
// [DOWN-REGULATION] (overview 6d) Forward a DOWN packet on `outDev`.
//   Every DOWN packet this routing forwards goes through here (RouteDownRing,
//   RouteNormalDown). Once the steady DOWN level of this satellite has been
//   recorded (end of the fill phase), the target queue is first topped up
//   with dummy packets:
//     - DOWN queue (devDown): to the own steady DOWN level m_downBaseOcc
//       (at most the ISL queue size, so the QUEUE_BUFFER headroom is kept);
//     - left queue (devLeft) of a NORMAL DOWN ring satellite (orbits 0-3,
//       forward cross link): to ISL queue size - 1, the size of the dummy bar
//       inserted at activation (InsertDummyPackets(GetIslQueueSize(), ...)).
//   The packet itself is enqueued behind the dummies. A queue that already
//   holds at least that many packets is not touched, so normal operation
//   (steady levels, Doppler ramps) inserts nothing. Dummies are dropped by
//   the next satellite: no stored copy is created or lost, only queueing
//   delay is restored.
//   Typical activity: ~4000 dummies per satellite of orbit 4 once per DOWN
//   switch (drained by [SEAM-FLUSH]; logged), small top-ups at the previous
//   ring positions after a switch (not logged), the cross-link bars of the
//   initial DOWN ring at the end of the fill phase (logged).
// ─────────────────────────────────────────────────────────────────────────────
void RoutingRingSwitchStar::ForwardDown(Ptr<NetDevice> outDev, Ptr<Packet> pkt, RingRole role)
{
#if DOWN_QUEUE_REGULATION
    if (m_downBaseRecorded) {
        // queue level to restore before this packet is enqueued (0 = none)
        uint64_t level = 0;
        if (outDev == m_app->GetDevDown())
            level = std::min<uint64_t>(m_downBaseOcc, m_app->GetIslQueueSize());
        else if (outDev == m_app->GetDevLeft() && role == RingRole::NORMAL
                 && m_app->GetIslQueueSize() > 0)
            level = m_app->GetIslQueueSize() - 1;

        if (level > 0) {
            Ptr<Queue<Packet>> q = DynamicCast<PointToPointLaserNetDevice>(outDev)->GetQueue();
            const uint64_t before = q->GetCurrentSize().GetValue();
            if (before < level) {
                // InsertDummyPackets fills up to (amount - 1) packets
                InsertDummyPackets((uint32_t) (level + 1), outDev);
                if (level - before >= DOWN_REGULATION_LOG_MIN)
                    printf("[RingSwitchStar] t=%.3fs  sat%u: %s queue topped up %lu -> %lu (dummies)\n",
                           Simulator::Now().GetSeconds(), m_app->GetSatId(),
                           outDev == m_app->GetDevDown() ? "DOWN" : "DOWN-ring left",
                           (unsigned long) before,
                           (unsigned long) q->GetCurrentSize().GetValue());
            }
        }
    }
#endif
    m_app->ForwardPacket(outDev, pkt, true);
}

// ─────────────────────────────────────────────────────────────────────────────
// UP switch (overview 4): initiation, activation, retirement
// ─────────────────────────────────────────────────────────────────────────────

// Called by the scheduler (RingSwitchScheduler::FireRingUpSwitch via
// SatelliteForwardingApp::TriggerRingUpSwitch) on the NEW seam-right m(4),
// one position below the current seam-right n(4).
// [UP-SHORTCUT] From now on m(4) forwards the lap traffic arriving from below
// to the left (RouteUpRing SEAM_RIGHT, devDown -> devLeft), one hop before
// that traffic would have reached n(4): those copies do 10 instead of 11 hops
// in orbit 4, once. Every such copy carries the new epoch and activates the
// new ring satellites along the new return row.
void RoutingRingSwitchStar::InitiateRingUpSwitch()
{
    // New epoch = newest epoch ever seen + 1; this satellite is ring from now on.
    m_maxUpEpochSeen = m_maxUpEpochSeen + 1;
    m_app->isRingUp = true;
    printf("[RingSwitchStar] t=%.3fs  sat%u: INITIATING ring-UP wave\n",
           Simulator::Now().GetSeconds(), m_app->GetSatId());
}

// A satellite at position n-1 that receives the new epoch on devRight (new
// return row) becomes ring. Called from OnReceive, which also inserts the
// cross-link dummies [DUMMIES] and routes the trigger packet by ring rules.
// [UP-SHORTCUT] As in orbit 4, lap traffic reaching this satellite from below
// now leaves the orbit here, one hop before the old ring satellite.
void RoutingRingSwitchStar::ActivateNewUpRing(uint32_t epoch)
{

    if (m_app->isSeamLeft) {
        // New seam-left (orbit 5): end of the activation wave along the new
        // return row. Completion of the switch is signalled later by the
        // retirement of the old seam-left (RetireUpRing).
        m_app->isRingUp = true;
        printf("[RingSwitchStar] t=%.3fs  sat%u: ring-UP activation reached new seam-left (epoch %u)\n",
               Simulator::Now().GetSeconds(), m_app->GetSatId(), epoch);
    } else {
        m_app->isRingUp = true;
        printf("[RingSwitchStar] t=%.3fs  sat%u: activated as new ring-up (epoch %u)\n",
               Simulator::Now().GetSeconds(), m_app->GetSatId(), epoch);
    }
}

// An old ring satellite n(p) that receives the new epoch from below (devDown):
// a copy that entered orbit p at the new ring satellite m(p), one hop below.
// From now on n(p) forwards by normal rules (the trigger packet continues
// upwards, i.e. completes its lap). Late traffic on its ring ports is handled
// by RouteNormalUp [LATE-TRAFFIC].
void RoutingRingSwitchStar::RetireUpRing(RingRole actingRole, uint32_t epoch)
{
    m_app->isRingUp = false;
    printf("[RingSwitchStar] t=%.3fs  sat%u: ring-UP retired\n",
           Simulator::Now().GetSeconds(), m_app->GetSatId());

    if (m_app->isSeamLeft && m_app->m_ringScheduler){
        m_app->m_ringScheduler->OnRingUpSwitchComplete();
        printf("[RingSwitchStar] UP switch terminated\n");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// DOWN switch (overview 5): initiation, activation, retirement
// ─────────────────────────────────────────────────────────────────────────────

// Called by the scheduler (RingSwitchScheduler::FireRingDownSwitch via
// SatelliteForwardingApp::TriggerRingDownSwitch) on the NEW seam-right m(4),
// one position below (= one hop AFTER, in DOWN direction) the current
// seam-right n(4).
// [DOWN-LAP-SKIP] Its devUp input is the old return stream that n(4) puts into
// orbit 4 one hop above. From now on m(4) forwards everything arriving from
// above to the left (RouteDownRing SEAM_RIGHT, devUp -> devLeft): these copies
// leave orbit 4 after 1 hop instead of a full lap. This continues until the
// old return stream ends (when the old seam-left in orbit 5 has retired and
// the old row is empty, ~0.3 s). Every forwarded copy carries the new epoch
// and activates the new ring satellites along the new forward row.
// [SEAM-FLUSH] Nothing enters orbit 4's lap until the new return stream
// reaches m(4) on devLeft, so all DOWN queues of orbit 4 drain meanwhile.
// [DOWN-REGULATION] restores each of them (ForwardDown) just before the first
// packet of the new return stream enters it.
void RoutingRingSwitchStar::InitiateRingDownSwitch()
{
    // New epoch = newest epoch ever seen + 1; this satellite is ring from now on.
    m_maxDownEpochSeen = m_maxDownEpochSeen + 1;
    m_app->isRingDown = true;

    printf("[RingSwitchStar] t=%.3fs  sat%u: INITIATING ring-DOWN switch (new seam-right active, epoch %u, flagging data stream)\n",
           Simulator::Now().GetSeconds(), m_app->GetSatId(), m_maxDownEpochSeen);
}

// A satellite at position n-1 that receives the new epoch on devRight (new
// forward row) becomes ring (Alg. 1 l. 6-11). Called from OnReceive, which also
// inserts the cross-link dummies [DUMMIES] and routes the trigger packet by
// ring rules (it enters this orbit: devRight -> devDown).
// [DOWN-LAP-SKIP] The old ring satellite n(p) is one hop above. Copies that
// still enter orbit p at n(p) via the old forward row now reach this satellite
// from above and leave the orbit here (devUp -> devLeft, seam-left: devUp ->
// devRight), after 1 hop instead of a full lap. These are the copies that did
// their lap in orbit p+1 on the old ring; the stream lasts about one lap of
// orbit p+1.
void RoutingRingSwitchStar::ActivateNewDownRing(uint32_t epoch)
{
    if (m_app->isSeamLeft) {
        // New seam-left (orbit 5): end of the activation wave along the new
        // forward row (writeup sO-1,n+1). From here the new return stream
        // starts. Completion is signalled by the retirement of the old
        // seam-left (RetireDownRing).
        m_app->isRingDown = true;
        printf("[RingSwitchStar] t=%.3fs  sat%u: ring-DOWN activation reached new seam-left (epoch %u)\n",
               Simulator::Now().GetSeconds(), m_app->GetSatId(), epoch);
    } else {
        m_app->isRingDown = true;
        printf("[RingSwitchStar] t=%.3fs  sat%u: activated as new ring-down (epoch %u)\n",
               Simulator::Now().GetSeconds(), m_app->GetSatId(), epoch);
    }
}

// An old ring satellite n(p) that receives the new epoch from above (devUp):
// a copy that entered orbit p at the new ring satellite m(p) and has gone
// around the orbit (10 hops). n(p) retires and passes it on downwards to m(p)
// (11th hop), where it leaves the orbit (Alg. 1 l. 17-21). With this, orbit p
// is back to full laps. The old seam-right n(4) retires last; condition
// (9)/(10) requires that the old return stream has ended by then (with queues
// this needs [DOWN-REGULATION], overview 6b/6d). Late traffic on the ring ports of a retired
// satellite is handled by RouteNormalDown [LATE-TRAFFIC].
void RoutingRingSwitchStar::RetireDownRing(RingRole actingRole, uint32_t epoch)
{
    // Flagged packet with newer epoch arrived on devUp → retire immediately
    m_app->isRingDown = false;
    printf("[RingSwitchStar] t=%.3fs  sat%u: ring-DOWN retired\n",
           Simulator::Now().GetSeconds(), m_app->GetSatId());

    if (m_app->isSeamLeft && m_app->m_ringScheduler){
        m_app->m_ringScheduler->OnRingDownSwitchComplete();
        printf("[RingSwitchStar] DOWN switch terminated\n");
    }
}


// ─────────────────────────────────────────────────────────────────────────────
// Second copies (content-fill-double, writeup 2.2.2)
//   An object whose second copy could not be inserted at creation time is
//   flagged (dup_code > 0). Every satellite that forwards a flagged packet
//   tries to create the missing copy in the other direction. Called from
//   OnReceive with the direction of the MISSING copy. Returns true if the copy
//   was created (the caller then clears the flag).
//   Only during the fill phase (STORAGE_FILL_PHASE_END_S): afterwards the
//   storage budget is fixed, and second copies created later refilled queues
//   that a ring switch had emptied (and seeded knock-on lumps).
// ─────────────────────────────────────────────────────────────────────────────
bool RoutingRingSwitchStar::DuplicatePacket(direction_t direction, uint32_t id, uint16_t origin,
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
    // Periodic storage: second copies are only created during the fill phase,
    // otherwise they add packets after the storage budget has been frozen
    // (they refilled queues emptied by a switch and seeded knock-on lumps).
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
            RingSwitchStarHeader hdr_new;
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
// Routing tables ([RING-RULES], overview 2)
//   `device` is the device the packet ARRIVED on. A packet that arrives on
//   devUp comes from the satellite above (it was sent downwards), a packet that
//   arrives on devDown comes from below (sent upwards).
// ─────────────────────────────────────────────────────────────────────────────

// A packet that arrived on a port with no rule is NOT forwarded, i.e. lost:
// count it as dropped (otherwise such losses never show up in the statistics).
void RoutingRingSwitchStar::LogUnknownDevice(int code, uint32_t last_hop)
{
    // The packet is NOT forwarded, i.e. it is lost -> count it, otherwise these
    // losses never show up in the packet statistics.
    m_app->num_dropped_packets++;
    FILE* f = fopen(filename_packet_monitoring, "a");
    if (f) {
        fprintf(f, "%u,%u,%lf,%d\n", m_app->GetSatId(), last_hop, Simulator::Now().GetSeconds(), code);
        fclose(f);
    }
}

// UP copies at a satellite that is acting as UP ring.
void RoutingRingSwitchStar::RouteUpRing(RingRole role, Ptr<NetDevice> device, Ptr<Packet> pkt, uint32_t last_hop)
{

    switch (role) {
    case RingRole::SEAM_LEFT:
        // orbit 5: start of the UP forward direction, end of the return stream
        //   devRight -> devUp    return stream ends: copy enters orbit 5
        //   devDown  -> devRight lap done -> forward to orbit 0
        //                        [UP-SHORTCUT] at a NEW seam-left this traffic
        //                        leaves one hop before the old seam-left
        //   devUp    -> devDown  reverse direction, not used by UP traffic
        if      (device == m_app->GetDevUp())    m_app->ForwardPacket(m_app->GetDevDown(),  pkt, true);
        else if (device == m_app->GetDevDown())  m_app->ForwardPacket(m_app->GetDevRight(), pkt, true);
        else if (device == m_app->GetDevRight()) m_app->ForwardPacket(m_app->GetDevUp(), pkt, true);
        else                                     LogUnknownDevice(4, last_hop);
        break;

    case RingRole::SEAM_RIGHT:
        // orbit 4: end of the UP forward direction, start of the return stream
        //   devLeft  -> devUp    copy enters orbit 4
        //   devDown  -> devLeft  lap done -> start of the return stream
        //                        [UP-SHORTCUT] at the NEW seam-right (initiator)
        //                        this traffic leaves one hop before the old one
        //   devUp    -> devDown  reverse direction, not used by UP traffic
        if      (device == m_app->GetDevUp())    m_app->ForwardPacket(m_app->GetDevDown(), pkt, true);
        else if (device == m_app->GetDevDown())  m_app->ForwardPacket(m_app->GetDevLeft(), pkt, true);
        else if (device == m_app->GetDevLeft())  m_app->ForwardPacket(m_app->GetDevUp(), pkt, true);
        else                                     LogUnknownDevice(3, last_hop);
        break;

    case RingRole::NORMAL:
    default:
        // orbits 0-3
        //   devLeft  -> devUp    copy enters this orbit
        //   devDown  -> devRight lap done -> forward to the next orbit
        //                        [UP-SHORTCUT] at a NEW ring satellite this
        //                        traffic leaves one hop before the old one
        //   devRight -> devLeft  return stream passes through
        //   devUp    -> devDown  reverse direction, not used by UP traffic
        if      (device == m_app->GetDevUp())    m_app->ForwardPacket(m_app->GetDevDown(),  pkt, true);
        else if (device == m_app->GetDevDown())  m_app->ForwardPacket(m_app->GetDevRight(), pkt, true);
        else if (device == m_app->GetDevLeft())  m_app->ForwardPacket(m_app->GetDevUp(), pkt, true);
        else if (device == m_app->GetDevRight()) m_app->ForwardPacket(m_app->GetDevLeft(),  pkt, true);
        break;
    }
}

// DOWN copies at a satellite that is acting as DOWN ring. All DOWN packets are
// sent through ForwardDown ([DOWN-REGULATION]: DOWN queue and, for NORMAL ring
// satellites, the left queue are topped up to their steady level first).
void RoutingRingSwitchStar::RouteDownRing(RingRole role, Ptr<NetDevice> device, Ptr<Packet> pkt, uint32_t last_hop)
{
    switch (role) {
    case RingRole::SEAM_LEFT:
        // orbit 5: end of the DOWN forward direction, start of the return stream
        //   devRight -> devDown  copy enters orbit 5
        //   devUp    -> devRight lap done -> start of the return stream
        //                        [DOWN-LAP-SKIP] at a NEW seam-left, copies that
        //                        entered orbit 5 at the old seam-left one hop
        //                        above arrive here too and leave orbit 5 after
        //                        1 hop (until the old forward row is empty)
        //   devDown  -> devUp    reverse direction, not used by DOWN traffic
        if      (device == m_app->GetDevUp())    ForwardDown(m_app->GetDevRight(), pkt, role);
        else if (device == m_app->GetDevDown())  ForwardDown(m_app->GetDevUp(),    pkt, role);
        else if (device == m_app->GetDevRight()) ForwardDown(m_app->GetDevDown(),  pkt, role);
        else                                     LogUnknownDevice(2, last_hop);
        break;

    case RingRole::SEAM_RIGHT:
        // orbit 4: start of the DOWN forward direction, end of the return stream
        //   devLeft  -> devDown  return stream ends: copy enters orbit 4. This is
        //                        the ONLY input of orbit 4's lap.
        //   devUp    -> devLeft  lap done -> forward to orbit 3
        //                        [DOWN-LAP-SKIP] at the NEW seam-right
        //                        (initiator): the old return stream that the old
        //                        seam-right puts into orbit 4 one hop above
        //                        arrives here and leaves orbit 4 after 1 hop.
        //                        [SEAM-FLUSH] meanwhile nothing enters orbit 4's
        //                        lap until the new return stream arrives on
        //                        devLeft, so orbit 4's DOWN queues drain.
        //                        [DOWN-REGULATION] the first packet of the new
        //                        return stream (devLeft -> devDown) and every
        //                        following hop in orbit 4 top the drained
        //                        DOWN queues up again before entering them.
        //   devDown  -> devUp    reverse direction, not used by DOWN traffic
        if      (device == m_app->GetDevUp())    ForwardDown(m_app->GetDevLeft(), pkt, role);
        else if (device == m_app->GetDevDown())  ForwardDown(m_app->GetDevUp(),   pkt, role);
        else if (device == m_app->GetDevLeft())  ForwardDown(m_app->GetDevDown(), pkt, role);
        else                                     LogUnknownDevice(1, last_hop);
        break;

    case RingRole::NORMAL:
    default:
        // orbits 0-3
        //   devRight -> devDown  copy enters this orbit
        //   devUp    -> devLeft  lap done -> forward to the next orbit (left)
        //                        [DOWN-LAP-SKIP] at a NEW ring satellite, copies
        //                        that entered this orbit at the old ring
        //                        satellite one hop above arrive here too and
        //                        leave the orbit after 1 hop (until the old
        //                        forward row is empty)
        //   devLeft  -> devRight return stream passes through
        //   devDown  -> devUp    reverse direction, not used by DOWN traffic
        if      (device == m_app->GetDevUp())    ForwardDown(m_app->GetDevLeft(),  pkt, role);
        else if (device == m_app->GetDevDown())  ForwardDown(m_app->GetDevUp(),    pkt, role);
        else if (device == m_app->GetDevLeft())  ForwardDown(m_app->GetDevRight(), pkt, role);
        else if (device == m_app->GetDevRight()) ForwardDown(m_app->GetDevDown(),  pkt, role);
        break;
    }
}

// UP copies at a satellite that is NOT acting as UP ring.
void RoutingRingSwitchStar::RouteNormalUp(Ptr<NetDevice> device, Ptr<Packet> pkt, uint32_t last_hop)
{
    // Normal satellite: devDown↔devUp pass-through
    if      (device == m_app->GetDevDown()) m_app->ForwardPacket(m_app->GetDevUp(),   pkt, true);
    else if (device == m_app->GetDevUp())   m_app->ForwardPacket(m_app->GetDevDown(), pkt, true);
    // [LATE-TRAFFIC] traffic on the ring ports of a retired up-ring satellite:
    //   from left  = old forward stream  -> enter this orbit (as the ring did)
    //   from right = old return stream   -> keep going left, except at the old
    //                terminus (seam-left): put it into the orbit, never cross the seam
    else if (device == m_app->GetDevLeft()) m_app->ForwardPacket(m_app->GetDevUp(),   pkt, true);
    else if (device == m_app->GetDevRight())
    {
        if (m_app->isSeamLeft) m_app->ForwardPacket(m_app->GetDevUp(),   pkt, true);
        else                   m_app->ForwardPacket(m_app->GetDevLeft(), pkt, true);
    }
    else                                    LogUnknownDevice(3, last_hop);
}

// DOWN copies at a satellite that is NOT acting as DOWN ring (sent through
// ForwardDown: [DOWN-REGULATION] tops the DOWN queue up first).
void RoutingRingSwitchStar::RouteNormalDown(Ptr<NetDevice> device, Ptr<Packet> pkt, uint32_t last_hop)
{
    // Normal satellite: devUp -> devDown pass-through (devDown -> devUp: reverse
    // direction, not used by DOWN traffic)
    if      (device == m_app->GetDevDown()) ForwardDown(m_app->GetDevUp(),   pkt, RingRole::NONE);
    else if (device == m_app->GetDevUp())   ForwardDown(m_app->GetDevDown(), pkt, RingRole::NONE);
    // [LATE-TRAFFIC] traffic on the ring ports of a retired down-ring satellite:
    //   from left  = old return stream  -> keep going right, except at the old
    //                terminus (seam-right): put it into the orbit. The right
    //                link of a seam-right satellite is the CROSS-SEAM ISL; using
    //                it created the endless loop through the up-ring row.
    //   from right = old forward stream -> enter this orbit (as the ring did).
    //                [DOWN-LAP-SKIP] the next hop is the new ring satellite, so
    //                these copies leave the orbit there after 1 hop.
    else if (device == m_app->GetDevLeft())
    {
        if (m_app->isSeamRight) ForwardDown(m_app->GetDevDown(),  pkt, RingRole::NONE);
        else                    ForwardDown(m_app->GetDevRight(), pkt, RingRole::NONE);
    }
    else if (device == m_app->GetDevRight()) ForwardDown(m_app->GetDevDown(), pkt, RingRole::NONE);
    else                                    LogUnknownDevice(0, last_hop);
}

// ─────────────────────────────────────────────────────────────────────────────
// OnReceive: called for every packet that arrives on one of the 4 ISLs.
//   1. dummies are dropped, TTL-expired packets are dropped
//   2. own packets refresh their PacketRecord (lastSeen, RTT statistics)
//   3. DOWN / UP packets: switch bookkeeping (epoch, activation, retirement,
//      queue preservation), second copies, epoch stamping, routing
//   4. BROADCAST packets: flooding with duplicate suppression
// ─────────────────────────────────────────────────────────────────────────────
bool RoutingRingSwitchStar::OnReceive(Ptr<NetDevice>    device,
                                  Ptr<const Packet> packet,
                                  uint16_t          protocol,
                                  const Address&    sender)
{
    if (protocol != SatelliteForwardingApp::PROTO &&
        protocol != SatelliteForwardingApp::PROTO_BROADCAST)
            return false;

    Ptr<Packet> pkt = packet->Copy();
    RingSwitchStarHeader hdr;
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

    // [DUMMIES] Dummy packets are just for timing, drop them silently
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

    // Own packet back at its origin: refresh its record (lastSeen: either
    // copy) and measure the RTT of this copy (time since the same copy was
    // last here; CheckPackets writes the mean per stats interval).
    double curr_time = Simulator::Now().GetSeconds();
    if (origin == sat_id && direction != BROADCAST) {
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

    if (direction == DOWN){
#if DOWN_QUEUE_REGULATION
        // [DOWN-REGULATION] steady DOWN level of this satellite = occupancy of
        // its DOWN queue at the first DOWN packet after the fill phase
        // (STORAGE_FILL_PHASE_END_S). From then on ForwardDown keeps the DOWN
        // queue and, as NORMAL ring satellite, the left queue at their level.
        if (!m_downBaseRecorded && Simulator::Now().GetSeconds() >= STORAGE_FILL_PHASE_END_S) {
            m_downBaseOcc      = DynamicCast<PointToPointLaserNetDevice>(m_app->GetDevDown())
                                     ->GetQueue()->GetCurrentSize().GetValue();
            m_downBaseRecorded = true;
        }
#endif

        // ── Down-ring switch: epoch tracking, activation, retirement ────────
        // Every satellite passively learns the current switch epoch from the
        // traffic passing through it; the initiator uses maxSeen+1.
        RingRole actingDown = ComputeDownRole();
        if (epoch  > m_maxDownEpochSeen) {
            // printf("EPOCH CHANGED %u --> %u Sat: %u Time: %lf\n", m_maxDownEpochSeen, pktEpoch, sat_id, Simulator::Now().GetSeconds());
            m_maxDownEpochSeen = epoch;

            if (actingDown == RingRole::NONE && device == m_app->GetDevRight()) {
                // Newer epoch from the right neighbour: this satellite is in the
                // new ring row -> activate. The trigger packet is routed by the
                // new ring rules below (Alg. 1 l. 6-11).
                // [DOWN-LAP-SKIP] from now on copies arriving from above (they
                // entered this orbit at the old ring satellite one hop above)
                // leave the orbit here.
                ActivateNewDownRing(epoch);
                // [DUMMIES] 6a: dummy bar on the new forward cross link, filled
                // up to the ISL queue size (original rule; the same level
                // [DOWN-REGULATION] keeps the old cross link at, so old and new
                // cross link carry the same queueing delay).
                InsertDummyPackets(m_app->GetIslQueueSize(), m_app->GetDevLeft());
            } else if (actingDown != RingRole::NONE && device == m_app->GetDevUp()) {
                // Newer epoch from above: a copy that entered this orbit at the
                // new ring satellite and went around the orbit -> retire NOW.
                // The trigger packet is routed by normal rules below (passes on
                // downwards to the new ring satellite; Alg. 1 l. 17-21).
                RetireDownRing(actingDown, epoch);
            }
        }
        // Epoch of the last DOWN packet seen (also used by the content
        // strategy: epoch of new packets, FREEZE_AFTER_FIRST_DOWN_SWITCH).
        m_app->m_currDownEpoch = epoch;

        // Flagged DOWN copy: try to create the missing UP copy (fill phase only)
        if (dup_code > 0){
            if (DuplicatePacket(UP, id, origin, ttl, obj_id, frag_id, time)){
                hdr.SetDupCode(0);
            }
        }
        actingDown = ComputeDownRole();
        // Epoch stamping: every packet forwarded by DOWN-ring rules carries this
        // satellite's epoch.
        if (actingDown != RingRole::NONE ) {
            hdr.SetEpoch(m_maxDownEpochSeen);
        }

        // DEBUG -> add last hop to see where this packet comes from
        hdr.SetLastHop(sat_id);
        pkt->AddHeader(hdr);

        // ── Routing ─────────────────────────────────────────────────────────────
        if      (actingDown != RingRole::NONE) RouteDownRing(actingDown, device, pkt, last_hop);
        else                                   RouteNormalDown(device, pkt, last_hop);

    } else if (direction == UP){
        // ── Up-ring switch: epoch tracking, activation, retirement ────────────
        // Every satellite passively learns the current switch epoch from the
        // traffic passing through it; the initiator uses maxSeen+1.
        RingRole actingUp= ComputeUpRole();
        if (epoch  > m_maxUpEpochSeen) {
            m_maxUpEpochSeen = epoch;

            if (actingUp == RingRole::NONE && device == m_app->GetDevRight()) {
                // Newer epoch from the right neighbour (new return row): this
                // satellite is in the new ring row -> activate. The trigger
                // packet is routed by the new ring rules below (continues left).
                // [UP-SHORTCUT] from now on lap traffic coming up the orbit
                // leaves here, one hop before the old ring satellite.
                ActivateNewUpRing(epoch);
                // [DUMMIES] 6a: the new cross link gets the queueing delay of
                // the old path, i.e. the own UP queue (not the full ISL size).
                InsertDummyPackets(DynamicCast<PointToPointLaserNetDevice>(m_app->GetDevUp())
                                       ->GetQueue()->GetCurrentSize().GetValue(),
                                   m_app->GetDevRight());
            } else if (actingUp != RingRole::NONE && device == m_app->GetDevDown()) {
                // Newer epoch from below: a copy that entered this orbit at the
                // new ring satellite one hop below -> retire NOW. The trigger
                // packet is routed by normal rules below (continues upwards).
                RetireUpRing(actingUp, epoch);
            }
        }
        m_app->m_currUpEpoch = m_maxUpEpochSeen;

        // Flagged UP copy: try to create the missing DOWN copy (fill phase only)
        if (dup_code > 0){
            if(DuplicatePacket(DOWN, id, origin, ttl, obj_id, frag_id, time)){
                hdr.SetDupCode(0);
            }
        }

        actingUp = ComputeUpRole();
        // Epoch stamping: every packet forwarded by UP-ring rules carries this
        // satellite's epoch.
        if (actingUp != RingRole::NONE) {
            hdr.SetEpoch(m_maxUpEpochSeen);
        }

        // DEBUG -> add last hop to see where this packet comes from
        hdr.SetLastHop(sat_id);
        pkt->AddHeader(hdr);

        // ── Routing ─────────────────────────────────────────────────────────────
        if      (actingUp != RingRole::NONE) RouteUpRing(actingUp, device, pkt, last_hop);
        else                                 RouteNormalUp(device, pkt, last_hop);

    } else if (direction == BROADCAST && protocol == SatelliteForwardingApp::PROTO_BROADCAST){
        // SIR, epidemic flooding
        if (origin == sat_id) return true;

        std::vector<Ptr<NetDevice>> devices = {
            m_app->GetDevUp(),
            m_app->GetDevDown(),
            m_app->GetDevLeft(),
            m_app->GetDevRight(),
        };


        uint64_t packet_id = ((uint64_t) origin << 32) | id;
        if (SeenBefore (origin, id)) return true; // CHANGE !!!
        if (dup_code > 0) { // Log the time the satellite received the packet only
                            // for some packets such that we do not slow down the simulation too much
            FILE* f = fopen(m_app->filename_broadcast_stats, "a");
            if (f) {
                fprintf(f, "0,%u,%u,%lu,%lf\n", sat_id, origin, id, Simulator::Now().GetSeconds());
                fclose(f);
            }
        }

        hdr.SetLastHop(sat_id);
        pkt->AddHeader(hdr);

        // Forward to all neighbours if possible
        // TODO: do we need to cut it off at certain latitudes towards the poles?
        Ptr<MobilityModel> mob = m_app->GetNode ()->GetObject<MobilityModel> ();
        Vector pos = mob->GetPosition ();
        double lat = std::asin (pos.z / std::sqrt (pos.x*pos.x + pos.y*pos.y + pos.z*pos.z));
        for (auto dev : devices){
            if (dev == device) continue;
            if (std::abs(lat) > 60.0 * M_PI / 180.0 && (dev == m_app->GetDevLeft() || dev == m_app->GetDevRight())) continue;
            if (m_app->isSeamLeft && dev == m_app->GetDevLeft()) continue;
            else if (m_app->isSeamRight && dev == m_app->GetDevRight()) continue;
            m_app->ForwardPacket(dev, pkt->Copy(), true, SatelliteForwardingApp::PROTO_BROADCAST);
        }
    }

    return true;
}

} // namespace ns3
