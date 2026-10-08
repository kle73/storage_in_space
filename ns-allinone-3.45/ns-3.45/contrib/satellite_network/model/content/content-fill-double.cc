#include "content-fill-double.h"

#include "ns3/simulator.h"
#include "ns3/queue.h"
#include "ns3/point-to-point-laser-net-device.h"
#include "../satellite-forwarding-app.h"

#include "ns3/node.h"
#include "ns3/mobility-model.h"
#include <cmath>
#include <cstdio>

namespace ns3 {


Ptr<Packet>
ContentFillDouble::MakeFragment (uint32_t obj_id,
                                 uint32_t frag_id,
                                 uint16_t direction,
                                 uint32_t epoch,
                                 uint16_t dup_code,
                                 uint32_t id)
{
    Ptr<Packet> pkt = Create<Packet> (SatelliteForwardingApp::maxPayloadSize);
    RingSwitchStarHeader hdr;
    hdr.SetId       (id);
    hdr.SetSat      (m_app->GetSatId ());
    hdr.SetTime     (Simulator::Now().GetMilliSeconds());
    hdr.SetTTL      (TIME_TO_LIVE);
    hdr.SetDirection(direction);
    hdr.SetEpoch    (epoch);
    hdr.SetObjId    (obj_id);
    hdr.SetFragId   (frag_id);
    hdr.SetLastHop  (m_app->GetSatId()); 
    hdr.SetDupCode  (dup_code);
    pkt->AddHeader (hdr);
    return pkt;
}

void ContentFillDouble::GenerateBroadcastPacket (uint32_t obj_id, uint32_t num_packets, uint32_t curr_id)
{
    Ptr<NetDevice> devs[4] = { m_app->GetDevUp (),   m_app->GetDevDown (),
                               m_app->GetDevLeft (), m_app->GetDevRight () };

    const uint32_t base = curr_id;   // shared id block for this object

    uint16_t monitore = 0;
    if (Simulator::Now().GetSeconds() >= 1.5){
        uint32_t roll = m_app->uniform_rnd->GetInteger(0, 100);
        if (roll < 1){
            monitore = 1;
        }
    }

    for (Ptr<NetDevice> dev : devs)
    {
        if (!dev) continue;

        if (dev == m_app->GetDevLeft() || dev == m_app->GetDevRight()){

            if (m_app->isSeamLeft && dev == m_app->GetDevLeft()) continue;
            if (m_app->isSeamRight && dev == m_app->GetDevRight()) continue;

            Ptr<MobilityModel> mob = m_app->GetNode ()->GetObject<MobilityModel> ();
            Vector pos = mob->GetPosition ();
            double lat = std::asin (pos.z / std::sqrt (pos.x*pos.x + pos.y*pos.y + pos.z*pos.z));
            if (std::abs(lat) > 60.0 * M_PI / 180.0) continue;
        }

        Ptr<Queue<Packet>> bq =
            DynamicCast<PointToPointLaserNetDevice> (dev)->GetBroadcastQueue ();

        int64_t space = (int64_t) m_app->GetMaxIslQueueFillLevel ()
                      - (int64_t) bq->GetCurrentSize ().GetValue ();
        if ((int64_t) num_packets > space || space <= 0) continue;
        for (uint32_t i = 0; i < num_packets; i++)
        {
            Ptr<Packet> pkt = MakeFragment (obj_id, i, BROADCAST, 0, monitore, base + i);
            dev->Send (pkt, dev->GetBroadcast (),
                       SatelliteForwardingApp::PROTO_BROADCAST);
            if (monitore){
                FILE* f = fopen(m_app->filename_broadcast_stats, "a");
                if (f) {
                    uint32_t sat_id = m_app->GetSatId();
                    fprintf(f, "1,%u,%u,%lu,%lf\n", sat_id, sat_id, base+i, Simulator::Now().GetSeconds());
                    fclose(f);
                }
            }
        }
    }

}


// ── Generate ─────────────────────────────────────────────────────────────────
void ContentFillDouble::Generate ()
{

    // <output folder>/content/content_stats.csv (SatelliteForwardingApp::OutputPath)
    SatelliteForwardingApp::OutputPath ("content/content_stats.csv",
                                        filename_content_stats, sizeof (filename_content_stats));
    if (m_app->GetSatId() == 0){
        FILE* f = fopen (filename_content_stats,"w");
        if (f) { fprintf (f, "is_sender,node,id,time\n"); fclose (f); }
    }

    uint32_t offset = m_app->uniform_rnd->GetInteger (0, 10);
    Simulator::Schedule (Seconds(GENERATION_START_TIME) + MicroSeconds (10 + offset),
                        &ContentFillDouble::GenerateObject, this);
    Simulator::Schedule (Seconds(GENERATION_START_TIME) + MilliSeconds (10),
                        &ContentFillDouble::SendObject, this);
}

// ── GenerateObject ───────────────────────────────────────────────────────────
void ContentFillDouble::GenerateObject ()
{
    uint32_t roll = m_app->uniform_rnd->GetInteger (0, m_app->GetNumSatellites () - 1);
    // periodic storage: do not queue up objects the frozen budget will never
    // admit (pending_objects would otherwise grow without bound)
    if (roll < 1 && !(m_budgetFrozen && m_app->GetNumLiveRecords() >= m_storageBudget)) {
        uint32_t obj_packet_count = m_app->obj_size / SatelliteForwardingApp::maxPayloadSize;
        pending_objects.push_back (
            std::make_pair (m_app->current_obj_id, obj_packet_count));
            m_app->current_obj_id++;
        m_numPendingPackets += obj_packet_count;
    }
                                                                // oneweb: 160000
    if (m_app->current_packet_id + m_numPendingPackets < 4100000000) {
        uint32_t offset = m_app->uniform_rnd->GetInteger (0, 10); 
        Simulator::Schedule (MicroSeconds (2 + offset),                 // CHANGE
                            &ContentFillDouble::GenerateObject, this);
    } else {
        m_app->stop_generating_objects = true;
    }
}

// ── SendObject ───────────────────────────────────────────────────────────────
void ContentFillDouble::SendObject ()
{
    // Switch blackout: no new packets from 1 s before until
    // SWITCH_SAFETY_TIMEOUT after every ring switch.
    for (const auto& t_fire : m_app->switch_times){
        if (Simulator::Now().GetSeconds() >= t_fire - 1 && 
            Simulator::Now().GetSeconds() < t_fire + SWITCH_SAFETY_TIMEOUT) {
                Simulator::Schedule (MicroSeconds(20), &ContentFillDouble::SendObject, this);
                return;
        }
    }

    // Fixed storage budget (periodic operation), see content-fill-double.h:
    // freeze the number of own live records once the fill phase is over and,
    // with FREEZE_AFTER_FIRST_DOWN_SWITCH, the first DOWN switch has settled.
    // m_currDownEpoch is set by the routing to the epoch of the last DOWN
    // packet this satellite forwarded (>= 1 once the first DOWN switch reached it).
    if (!m_budgetFrozen && Simulator::Now().GetSeconds() >= STORAGE_FILL_PHASE_END_S) {
        bool freezeNow = true;
#if FREEZE_AFTER_FIRST_DOWN_SWITCH
        // wait until the first DOWN switch has settled (see content-fill-double.h)
        // (no scheduled switches at all -> freeze at STORAGE_FILL_PHASE_END_S)
        if (m_firstDownSwitchSeenAt < 0 && m_app->m_currDownEpoch >= 1)
            m_firstDownSwitchSeenAt = Simulator::Now().GetSeconds();
        // (closed Walker-Delta rings: freeze at STORAGE_FILL_PHASE_END_S, see
        //  content-fill-double.h)
        if (!m_app->switch_times.empty() && !m_app->GetTopoConfig().closedRing)
            freezeNow = m_firstDownSwitchSeenAt >= 0 &&
                        Simulator::Now().GetSeconds() >= m_firstDownSwitchSeenAt + FIRST_DOWN_SWITCH_SETTLE_S;
#endif
        if (freezeNow) {
            m_storageBudget = m_app->GetNumLiveRecords();
            m_budgetFrozen  = true;
        }
    }

    if (!pending_objects.empty()) {
        auto [obj_id, obj_pkt_count] = pending_objects.front ();

        if (m_budgetFrozen && m_app->GetNumLiveRecords() + obj_pkt_count > m_storageBudget) {
            // budget exhausted: only replace packets once own records expire (TTL)
            Simulator::Schedule (MilliSeconds (1), &ContentFillDouble::SendObject, this);
            return;
        }
        Ptr<NetDevice> dev_up = m_app->GetDevUp(); 
        Ptr<NetDevice> dev_down = m_app->GetDevDown(); 
        uint32_t base_id = m_app->current_packet_id;

// NEW !!!
        // for (uint32_t i = 0; i < obj_pkt_count; i++) {
            // uint32_t id = m_app->AllocatePacketId();
        // }
        // GenerateBroadcastPacket(obj_id, obj_pkt_count, base_id);
        // pending_objects.pop_front ();
        // m_numPendingPackets -= obj_pkt_count;

        // Simulator::Schedule (MicroSeconds (20), &ContentFillDouble::SendObject, this);
        // return;
// NEW END !!!

        // Admission: an object enters a queue only if the whole object fits
        // below the fill level (GetMaxIslQueueFillLevel) of that queue.
        // Ptr<NetDevice> device = m_app->GetDevUp();
        Ptr<Queue<Packet>> q_up =
            DynamicCast<PointToPointLaserNetDevice> (dev_up)->GetQueue ();
        Ptr<Queue<Packet>> q_down =
            DynamicCast<PointToPointLaserNetDevice> (dev_down)->GetQueue ();


        int64_t numPackets_qup = (int64_t)q_up->GetCurrentSize ().GetValue ();
        int64_t numPackets_qdown = (int64_t)q_down->GetCurrentSize ().GetValue ();

        int64_t space_left_up = (int64_t)m_app->GetMaxIslQueueFillLevel() - numPackets_qup;
        int64_t space_left_down = (int64_t)m_app->GetMaxIslQueueFillLevel() - numPackets_qdown;
        // no admission into a direction the routing has closed
        // (Walker-Delta kink orbit, see SatelliteForwardingApp::m_noInjectUp)
        if (m_app->m_noInjectUp)   space_left_up   = 0;
        if (m_app->m_noInjectDown) space_left_down = 0;


        // only the UP queue has space: UP copy only, flagged (dup_code 1)
        if ((int64_t)obj_pkt_count <= space_left_up && space_left_up > 0 &&
            !((int64_t)obj_pkt_count <= space_left_down && space_left_down > 0)) {

            for (uint32_t i = 0; i < obj_pkt_count; i++) {
                uint32_t id = m_app->AllocatePacketId();
                Ptr<Packet> pkt_up = MakeFragment(obj_id, i, UP, m_app->m_currUpEpoch, 1, id);
                dev_up->Send (pkt_up, dev_up->GetBroadcast (), SatelliteForwardingApp::PROTO);
            }

            { FILE* f = fopen(m_app->filename_obj_inject,"a");
              if (f){ fprintf(f,"%lf,%u,%u,%u,1\n", Simulator::Now().GetSeconds(),
                              m_app->GetSatId(), obj_id, obj_pkt_count); fclose(f); } }
            pending_objects.pop_front ();
            m_numPendingPackets -= obj_pkt_count;


        // only the DOWN queue has space: DOWN copy only, flagged (dup_code 1)
        } else if (!((int64_t)obj_pkt_count <= space_left_up && space_left_up > 0) &&
            (int64_t)obj_pkt_count <= space_left_down && space_left_down > 0){

            for (uint32_t i = 0; i < obj_pkt_count; i++) {
                uint32_t id = m_app->AllocatePacketId();
                Ptr<Packet> pkt_down = MakeFragment(obj_id, i, DOWN, m_app->m_currDownEpoch, 1, id);
                dev_down->Send (pkt_down, dev_down->GetBroadcast (), SatelliteForwardingApp::PROTO);
            }

            { FILE* f = fopen(m_app->filename_obj_inject,"a");
              if (f){ fprintf(f,"%lf,%u,%u,%u,2\n", Simulator::Now().GetSeconds(),
                              m_app->GetSatId(), obj_id, obj_pkt_count); fclose(f); } }

            pending_objects.pop_front ();
            m_numPendingPackets -= obj_pkt_count;

        // both queues have space: both copies (dup_code 0)
        } else if ((int64_t)obj_pkt_count <= space_left_up && space_left_up > 0 &&
            (int64_t)obj_pkt_count <= space_left_down && space_left_down > 0)
        {
            for (uint32_t i = 0; i < obj_pkt_count; i++) {
                uint32_t id = m_app->AllocatePacketId();
                Ptr<Packet> pkt_up = MakeFragment(obj_id, i, UP, m_app->m_currUpEpoch, 0, id);
                Ptr<Packet> pkt_down = MakeFragment(obj_id, i, DOWN, m_app->m_currDownEpoch, 0, id);
                dev_up->Send (pkt_up, dev_up->GetBroadcast (), SatelliteForwardingApp::PROTO);
                dev_down->Send (pkt_down, dev_down->GetBroadcast (), SatelliteForwardingApp::PROTO);
            }

            { FILE* f = fopen(m_app->filename_obj_inject,"a");
              if (f){ fprintf(f,"%lf,%u,%u,%u,0\n", Simulator::Now().GetSeconds(),
                              m_app->GetSatId(), obj_id, obj_pkt_count); fclose(f); } }

            pending_objects.pop_front ();
            m_numPendingPackets -= obj_pkt_count;
        }
        else
        {
            // no space in either queue: try again later
            Simulator::Schedule (MicroSeconds (20), &ContentFillDouble::SendObject, this);
            return;
        }

        // GenerateBroadcastPacket(obj_id, obj_pkt_count, base_id);
    }
    else if (m_app->stop_generating_objects)
    {
        FILE* f = fopen (m_app->filename_debug, "a");
        if (f) {
            fprintf (f, "STOP %u,%lf,%u\n",
                     m_app->GetSatId (),
                     Simulator::Now ().GetSeconds (),
                     m_app->current_packet_id);
            fclose (f);
        }
        return; // No more objects ever — stop rescheduling
    }

    Simulator::Schedule (MicroSeconds (10), &ContentFillDouble::SendObject, this);
}


} // namespace ns3
