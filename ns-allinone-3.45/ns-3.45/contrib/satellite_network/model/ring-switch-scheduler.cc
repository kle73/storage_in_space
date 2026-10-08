// RingSwitchScheduler.cc  –  mobility-sampled safe-switch timing (MPI-safe)
//
// Geometry is sampled from the NODES (mobility on every node on every rank),
// NOT from m_apps (rank-local, full of nulls). The inter-plane ISL of the ring
// at position `pos` in orbit k is the ACTUAL link installed at t=0 -- i.e. the
// nearest-neighbour match from Round B -- so we look the far endpoint up in
// m_islRightPartner rather than assuming same-index (which is false under Walker
// phasing). Links are fixed for the whole run, so one lookup per (k,pos) suffices.
//
// Safety condition (writeup (5) up / (6) down): for every adjacent orbit pair
// (k,k+1),  dist(ring ISL at pos_old) < dist(ring ISL at pos_new).  k runs
// 0..O-2, excluding the cross-seam pair.

#include "ring-switch-scheduler.h"

#include "ns3/mobility-model.h"                     // MobilityModel, CalculateDistance
#include "ns3/node.h"
#include "ns3/node-container.h"
#include "ns3/satellite-position-mobility-model.h"
#include "ns3/satellite.h"

#include <cstdint>
#include <stdio.h>

#define MAX_SCAN_S 7200
#define D_T 1

namespace ns3 {

// Position of `node`'s satellite at absolute simulation time `t` (seconds), in
// metres, ITRF. 
static Vector GetSatPosition (Ptr<Node> node, double t)
{
    if (!node) return Vector(0, 0, 0);
    Ptr<MobilityModel> mm = node->GetObject<MobilityModel>();
    if (!mm) return Vector(0, 0, 0);

    Ptr<SatellitePositionMobilityModel> sm =
        DynamicCast<SatellitePositionMobilityModel>(mm);
    if (!sm)
        return mm->GetPosition();                       // static run: frozen position

    Ptr<Satellite> sat = sm->GetSatellite();
    if (!sat)
        return Vector(0, 0, 0);

    JulianDate cur = sm->GetStartTime() + Seconds(t);   // == m_start + Now() at time t
    return sat->GetPosition(cur);                        // metres, ITRF; (0,0,0) on SGP4 error
}

namespace {

double IslDist (Ptr<Node> a, Ptr<Node> b, double t )
{
    Vector pa = GetSatPosition(a, t);
    Vector pb = GetSatPosition(b, t);
    return CalculateDistance(pa, pb);
}

} // anonymous namespace

// ── Setup ─────────────────────────────────────────────────────────────────────
void RingSwitchScheduler::Setup (ConstellationConfig                    config,
                                 uint32_t                               satsPerOrbit,
                                 uint32_t                               numOrbits,
                                 std::vector<SatelliteForwardingApp*>   apps,
                                 NodeContainer                          nodes,
                                 std::vector<uint32_t>                  islRightPartner,
                                 std::vector<std::vector<uint32_t>>     orbits,
                                 uint32_t                               systemId)
{
    m_config          = config;
    m_satsPerOrbit    = satsPerOrbit;
    m_numOrbits       = numOrbits;
    m_apps            = apps;
    m_nodes           = nodes;
    m_islRightPartner = islRightPartner;
    m_orbits          = orbits;
    m_systemId        = systemId;
}


//---- Helpers (quite inefficient) ----------------
std::pair<uint32_t, uint32_t> RingSwitchScheduler::FindSatIndex(uint32_t s){
    for (uint32_t o=0; o<m_numOrbits; ++o){
        for (uint32_t p=0; p<m_satsPerOrbit; ++p){
            if (m_orbits[o][p] == s){
                return std::pair(o, p);
            }
        }
    }
    return std::pair(UINT32_MAX, UINT32_MAX);
}

uint32_t RingSwitchScheduler::FindDownNeighbor(uint32_t s){
    auto idxs = FindSatIndex(s);
    uint32_t o = idxs.first;
    uint32_t p = (idxs.second == 0) ? m_satsPerOrbit - 1 : (idxs.second - 1);
    return m_orbits[o][p];
}

uint32_t RingSwitchScheduler::FindUpNeighbor(uint32_t s){
    auto idxs = FindSatIndex(s);
    uint32_t o = idxs.first;
    uint32_t p = (idxs.second + 1) % m_satsPerOrbit;
    return m_orbits[o][p];
}

bool RingSwitchScheduler::CheckSwitch(uint32_t srId, uint32_t slId, double t){
    uint32_t curr_ring = slId;
    uint32_t next_ring = FindDownNeighbor(slId);
    // Walker-Star: O-1 ring ISLs (the cross-seam pair is never used).
    // Walker-Delta (closed ring): also the ISL from the last to the first orbit.
    const uint32_t nLinks = m_config.closedRing ? m_numOrbits : m_numOrbits - 1;
    for (uint32_t o=0; o<nLinks; ++o){
        bool valid = false;
        double curr_dist = IslDist(m_nodes.Get(curr_ring), m_nodes.Get(m_islRightPartner[curr_ring]), 
                                   t);
        double next_dist = IslDist(m_nodes.Get(next_ring), m_nodes.Get(m_islRightPartner[next_ring]), 
                                   t);
        if (curr_dist >= next_dist) return false;
        curr_ring = m_islRightPartner[curr_ring];
        next_ring = m_islRightPartner[next_ring];
    }
    return true;
}

// ── Start ─────────────────────────────────────────────────────────────────────
void RingSwitchScheduler::Start ()
{
    const uint32_t S       = m_satsPerOrbit;
    const uint32_t nOrbits = static_cast<uint32_t>(m_nodes.GetN() / S);

    if (m_systemId == 0) {
        printf("\n[RingSwitchScheduler] ====== STARTUP ======\n");
        printf("[RingSwitchScheduler] %u orbits x %u sats  (ISL delays sampled from node mobility)\n",
               nOrbits, S);
        printf("[RingSwitchScheduler] safetyMargin=%.1fs  waveGuard=%.1fs\n",
               m_config.safetyMarginS, WAVE_GUARD_S);
        printf("[RingSwitchScheduler] Condition: dist(pos_old) < dist(pos_new) "
               "for all %u orbit pairs (actual matched ISLs)\n", nOrbits - 1);
    }

    // ── Ring-UP ───────────────────────────────────────────────────────────────
    {
        double t = 1;
        size_t step = 1;
        uint32_t srId = m_config.seamRightUp;
        uint32_t slId = m_config.seamLeftUp;
        while (t < MAX_SCAN_S){
            if (CheckSwitch(srId, slId, t)){
                double t_fire = t + m_config.safetyMarginS;
                if (m_systemId == 0){
                    printf("[RingSwitchScheduler] ring-UP step %2zu: sat%u->sat%u  t*=%.1fs\n",
                            step, srId, slId, t_fire);
                }
                srId = FindDownNeighbor(srId);
                slId = FindDownNeighbor(slId);
                Simulator::Schedule(Seconds(t_fire),
                                    &RingSwitchScheduler::FireRingUpSwitch,
                                    this, srId, slId, static_cast<uint32_t>(step));
                for (const auto& app : m_apps){
                    if (app != nullptr) app->switch_times.push_back(t_fire);
                }
                step++;
            }
            t += D_T;
        }
    }

    // ── Ring-DOWN ─────────────────────────────────────────────────────────────
    {

        double t = 1;
        size_t step = 1;
        uint32_t srId = m_config.seamRightDown;
        uint32_t slId = m_config.seamLeftDown;
        while (t < MAX_SCAN_S){
            if (CheckSwitch(srId, slId, t)){
                double t_fire = t + m_config.safetyMarginS;
                if (m_systemId == 0){
                    printf("[RingSwitchScheduler] ring-DOWN step %2zu: sat%u->sat%u  t*=%.1fs\n",
                            step, srId, slId, t_fire);
                }
                srId = FindDownNeighbor(srId);
                slId = FindDownNeighbor(slId);
                Simulator::Schedule(Seconds(t_fire),
                                    &RingSwitchScheduler::FireRingDownSwitch,
                                    this, srId, slId, static_cast<uint32_t>(step));
                for (const auto& app : m_apps){
                    if (app != nullptr) app->switch_times.push_back(t_fire);
                }
                step++;
            }
            t += D_T;
        }
    }

    if (m_systemId == 0)
        printf("[RingSwitchScheduler] ====== all switches pre-scheduled ======\n\n");
}

// ── Fire callbacks ────────────────────────────────────────────────────────────
void RingSwitchScheduler::FireRingUpSwitch (uint32_t initiatorId,
                                            uint32_t terminusId,
                                            uint32_t step)
{
    if (initiatorId < m_apps.size() && m_apps[initiatorId]) {
        printf("[RingSwitchScheduler] rank%u t=%.3fs FIRE ring-UP  step%u sat%u->sat%u\n",
               m_systemId, Simulator::Now().GetSeconds(), step, initiatorId, terminusId);
        m_apps[initiatorId]->TriggerRingUpSwitch();
    }
}

void RingSwitchScheduler::FireRingDownSwitch (uint32_t initiatorId,
                                              uint32_t terminusId,
                                              uint32_t step)
{
    if (initiatorId < m_apps.size() && m_apps[initiatorId]) {
        printf("[RingSwitchScheduler] rank%u t=%.3fs FIRE ring-DOWN step%u sat%u->sat%u\n",
               m_systemId, Simulator::Now().GetSeconds(), step, initiatorId, terminusId);
        m_apps[initiatorId]->TriggerRingDownSwitch(); 
    }
}

// ── Completion callbacks ──────────────────────────────────────────────────────
void RingSwitchScheduler::OnRingUpSwitchComplete ()
{
    printf("[RingSwitchScheduler] rank%u t=%.3fs ring-UP  wave COMPLETE at terminus\n",
           m_systemId, Simulator::Now().GetSeconds());
}

void RingSwitchScheduler::OnRingDownSwitchComplete ()
{
    printf("[RingSwitchScheduler] rank%u t=%.3fs ring-DOWN wave COMPLETE at terminus\n",
           m_systemId, Simulator::Now().GetSeconds());
}

} // namespace ns3