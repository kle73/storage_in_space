#ifndef RING_SWITCH_SCHEDULER_H
#define RING_SWITCH_SCHEDULER_H

#include "constellation-config.h"

#include "ns3/mobility-model.h"      // MobilityModel, CalculateDistance
#include "ns3/node.h"
#include "ns3/satellite.h"
#include "ns3/satellite-position-mobility-model.h"
#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/type-id.h"
#include "ns3/vector.h"
#include "ns3/node-container.h"
#include "satellite-forwarding-app.h"

#ifdef NS3_MPI
#include "ns3/mpi-interface.h"
#endif

#include <cstdint>
#include <vector>

namespace ns3 {

class SatelliteForwardingApp;

/**
 * Drives the continuous ring-switch cycle.
 *
 * Usage (scratch/storage_in_space.cc):
 *   RingSwitchScheduler sched;
 *   sched.Setup(MakeConstellationConfig(constellation), S, allApps);
 *   sched.Start();      // called after Simulator starts, before Run()
 *
 * The scheduler:
 *  1. Computes T* for the next initiator using orbital mechanics.
 *  2. Schedules TriggerRingUpSwitch / TriggerRingDownSwitch on that satellite.
 *  3. When the wave terminus calls OnRingUpSwitchComplete / OnRingDownSwitchComplete,
 *     the scheduler advances the step, updates terminus flags, and repeats.
 *
 * NOTE: callback-based completion detection works in single-rank mode only.
 * In MPI mode the terminus may be on a different rank; use the static-schedule
 * fallback (RING_SWITCH_UP_TIME_S) instead.
 */
class RingSwitchScheduler
{
public:
    // systemId: MPI rank of this process (0 for single-rank). Used to suppress
    // duplicate output — only rank 0 prints scheduler messages.
    void Setup (ConstellationConfig                    config,
                uint32_t                               satsPerOrbit,
                uint32_t                               numOrbits,
                std::vector<SatelliteForwardingApp*>   apps,
                NodeContainer                          nodes,
                std::vector<uint32_t>                  islRightPartner,
                std::vector<std::vector<uint32_t>>     orbits,
                uint32_t                               systemId = 0);

    // Call once before Simulator::Run().
    // Pre-schedules all switches in the sequence and sets all terminus flags
    // so no cross-rank callback is needed during the simulation (MPI-safe).
    void Start ();

    // Informational callbacks called by the terminus on its local rank.
    // Used only for printing — scheduling is already done in Start().
    void OnRingUpSwitchComplete   ();
    void OnRingDownSwitchComplete ();

private:

    std::pair<uint32_t, uint32_t> FindSatIndex(uint32_t s);
    uint32_t FindDownNeighbor(uint32_t s);
    uint32_t FindUpNeighbor(uint32_t s);
    bool CheckSwitch(uint32_t srId, uint32_t slId, double t);

    // Trampoline scheduled via Simulator::Schedule — fires only on the rank
    // that owns initiatorId (other ranks have a null pointer and do nothing).
    void FireRingUpSwitch   (uint32_t initiatorId, uint32_t terminusId, uint32_t step);
    void FireRingDownSwitch (uint32_t initiatorId, uint32_t terminusId, uint32_t step);

    ConstellationConfig                    m_config;
    uint32_t                               m_satsPerOrbit {0};
    uint32_t                               m_numOrbits{0};
    std::vector<SatelliteForwardingApp*>   m_apps;
    NodeContainer                          m_nodes;
    std::vector<uint32_t>                  m_islRightPartner;
    std::vector<std::vector<uint32_t>>     m_orbits;
    uint32_t                               m_systemId     {0};

    // Estimated wave propagation time added between consecutive scheduled switches.
    static constexpr double WAVE_GUARD_S = 5.0;
};

} // namespace ns3
#endif // RING_SWITCH_SCHEDULER_H
