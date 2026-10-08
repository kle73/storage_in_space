#ifndef CONTENT_FILL_DOUBLE_H
#define CONTENT_FILL_DOUBLE_H

#include "../strategy-interfaces.h"
#include "../routing/routing-ring-switch-walker-star.h"

#include <deque>
#include <utility>

#define CHECK_BROADCAST_TIME 0
#define GENERATION_START_TIME 0 // in seconds
// no new packets in [switch time - 1 s, switch time + SWITCH_SAFETY_TIMEOUT)
#define SWITCH_SAFETY_TIMEOUT 3 // in seconds
#define TIME_TO_LIVE 10000      // [s]; packets older than this are dropped

// ── Periodic storage (fixed budget) ──────────────────────────────────────────
// The queues are filled during an initial fill phase [0, STORAGE_FILL_PHASE_END_S)
// (admission rule: an object enters a queue only while that queue is below the
// fill level). Afterwards the stored amount is fixed: every satellite keeps at
// most as many own packets alive as it had when its budget was frozen (with a
// finite TTL, expired packets are replaced up to that budget). Without the
// fixed budget every ring switch increased the stored amount: a switch empties
// some queues and builds backlogs elsewhere; admission refilled the emptied
// queues while the backlogs never drain in a saturated loop.
// Also used for:
//   - second copies (RoutingRingSwitchStar::DuplicatePacket) stop here;
//   - each satellite records its steady DOWN queue level here, and the DOWN
//     queue regulation starts (RoutingRingSwitchStar [DOWN-REGULATION]; the
//     routing file has its own copy of this value, keep both equal).
#define STORAGE_FILL_PHASE_END_S 20.0
// The budget is frozen only once the FIRST DOWN ring switch has settled
// (FIRST_DOWN_SWITCH_SETTLE_S after the satellite has seen the new DOWN epoch).
// The initial DOWN ring has run for less than a full switch period when the
// first switch happens, i.e. it has accumulated less Doppler backlog than the
// switch gives back (the new ring is longer); the first switch therefore
// over-drains a few DOWN queues once. Admission refills them before the budget
// is frozen, so all later switches start from the steady state.
// (Only meaningful if the first DOWN switch comes before the first UP switch,
// as for Iridium; set to 0 to freeze at STORAGE_FILL_PHASE_END_S.)
// Not applied to closed rings (Walker-Delta, ConstellationConfig::closedRing):
// their budget is frozen at STORAGE_FILL_PHASE_END_S. A Walker-Delta DOWN
// switch takes one full ring revolution (several seconds, longer than the
// switch blackout); admission during it would fill the transient gaps of the
// switch and overflow queues when they close.
// Second copies (DuplicatePacket) still stop at STORAGE_FILL_PHASE_END_S.
#define FREEZE_AFTER_FIRST_DOWN_SWITCH 1
#define FIRST_DOWN_SWITCH_SETTLE_S     10.0

namespace ns3 {



/**
 * Continuous object generation with two copies per object (writeup 2.2.2).
 *
 * GenerateObject() runs every 2-12 us; with probability 1/N (N = number of
 *   satellites) it appends a new object (obj_size / maxPayloadSize packets) to
 *   pending_objects. Nothing is queued while the frozen budget is exhausted.
 * SendObject() runs every 10 us (20 us / 1 ms when it has to wait) and inserts
 *   the first pending object:
 *     - both queues below the fill level: UP copy and DOWN copy (dup_code 0)
 *     - only one queue below the fill level: that copy only, flagged
 *       (dup_code 1) so that DuplicatePacket can add the other copy later
 *       (fill phase only)
 *     - neither: wait.
 *   No insertion in the switch blackout window around every ring switch, and
 *   none beyond the frozen storage budget (see STORAGE_FILL_PHASE_END_S,
 *   FREEZE_AFTER_FIRST_DOWN_SWITCH).
 */
class ContentFillDouble : public ContentStrategy
{
public:
    void Generate () override;

private:


    Ptr<Packet> MakeFragment (uint32_t obj_id, uint32_t frag_id,
                                 uint16_t direction,
                                 uint32_t epoch,
                                 uint16_t dup_code,
                                 uint32_t id);
    uint32_t m_broadcastId {0};

    void GenerateObject ();
    void SendObject     ();

    void GenerateBroadcastPacket (uint32_t obj_id, uint32_t num_packets, uint32_t curr_id);

    std::deque<std::pair<uint32_t,uint32_t>> pending_objects;
    uint32_t m_numPendingPackets{0};

    // fixed storage budget = number of own live records (PacketRecords) when the
    // budget was frozen (STORAGE_FILL_PHASE_END_S / FREEZE_AFTER_FIRST_DOWN_SWITCH)
    uint64_t m_storageBudget {0};
    bool     m_budgetFrozen  {false};
    double   m_firstDownSwitchSeenAt {-1.0};   // time this satellite first saw DOWN epoch >= 1

    char filename_content_stats[512];
};

} // namespace ns3
#endif // CONTENT_FILL_DOUBLE_H
