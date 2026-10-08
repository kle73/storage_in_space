#ifndef RING_SWITCH_SCHEDULER_H
#define RING_SWITCH_SCHEDULER_H

#include "constellation-config.h"

#include "ns3/node-container.h"

#include <cstdint>
#include <vector>

namespace ns3
{

class SatelliteForwardingApp;

/**
 * Computes the times of the ring switches and starts them.
 *
 * A switch moves a ring row one position down in every orbit. It is due at
 * the first time t at which every ring ISL of the current row is shorter than
 * the ISL one position below; it is started safetyMarginS later on the new
 * seam-right ring satellite (TriggerRingUpSwitch / TriggerRingDownSwitch).
 * The rows are walked along the installed cross-plane ISLs, starting at the
 * seam-left satellite of the constellation config; closed rings also include
 * the ISL from the last to the first orbit.
 *
 * All switches are computed from the satellite orbits before the simulation
 * starts, so MPI ranks need no messages to agree on them, and every
 * application learns all switch times (AddRingSwitchTime).
 */
class RingSwitchScheduler
{
  public:
    /// `rightPartner[s]`: satellite at the other end of the right ISL of satellite `s`.
    /// `apps[s]`: application of satellite `s`, nullptr if it runs on another MPI rank.
    RingSwitchScheduler(const ConstellationConfig& config,
                        uint32_t satsPerOrbit,
                        uint32_t numOrbits,
                        NodeContainer nodes,
                        std::vector<uint32_t> rightPartner,
                        std::vector<SatelliteForwardingApp*> apps);

    /// Computes the switches of both rings up to `endTime` [s] and schedules them.
    void ScheduleSwitches(double endTime);

  private:
    enum class Ring
    {
        UP,
        DOWN
    };

    void ScheduleRing(Ring ring, double endTime);
    /// True if every ring ISL of the row starting at `rowStart` is shorter at
    /// time `t` [s] than the ISL one position below.
    bool IsSwitchDue(uint32_t rowStart, double t) const;
    /// Length of the right ISL of satellite `sat` at time `t` [m].
    double GetRightIslLength(uint32_t sat, double t) const;
    /// Next satellite downwards in the orbit of `sat`.
    uint32_t GetDownNeighbour(uint32_t sat) const;
    void FireSwitch(Ring ring, uint32_t initiator, uint32_t rowStart, uint32_t step);

    ConstellationConfig m_config;
    uint32_t m_satsPerOrbit;
    uint32_t m_numOrbits;
    NodeContainer m_nodes;
    std::vector<uint32_t> m_rightPartner;
    std::vector<SatelliteForwardingApp*> m_apps;
    uint32_t m_systemId; ///< MPI rank; only rank 0 prints the schedule
};

} // namespace ns3

#endif // RING_SWITCH_SCHEDULER_H
