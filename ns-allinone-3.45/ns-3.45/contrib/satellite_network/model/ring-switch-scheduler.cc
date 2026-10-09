#include "ring-switch-scheduler.h"

#include "satellite-forwarding-app.h"

#include "ns3/mobility-model.h"
#include "ns3/mpi-interface.h"
#include "ns3/node.h"
#include "ns3/satellite-position-mobility-model.h"
#include "ns3/satellite.h"
#include "ns3/simulator.h"

#include <cstdio>
#include <utility>

namespace ns3
{

namespace
{

/// The switch condition is checked every second from this time on [s].
constexpr double kScanStart = 1.0;
constexpr double kScanStep = 1.0;

/// Position of the satellite of `node` at simulation time `t` [s] (ITRF, metres).
Vector
GetPosition(Ptr<Node> node, double t)
{
    Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
    Ptr<SatellitePositionMobilityModel> orbit = DynamicCast<SatellitePositionMobilityModel>(mobility);
    if (!orbit)
    {
        return mobility->GetPosition(); // static positions (--forceStatic)
    }
    return orbit->GetSatellite()->GetPosition(orbit->GetStartTime() + Seconds(t));
}

} // namespace

RingSwitchScheduler::RingSwitchScheduler(const ConstellationConfig& config,
                                         uint32_t satsPerOrbit,
                                         uint32_t numOrbits,
                                         NodeContainer nodes,
                                         std::vector<uint32_t> rightPartner,
                                         std::vector<SatelliteForwardingApp*> apps)
    : m_config(config),
      m_satsPerOrbit(satsPerOrbit),
      m_numOrbits(numOrbits),
      m_nodes(std::move(nodes)),
      m_rightPartner(std::move(rightPartner)),
      m_apps(std::move(apps)),
      m_systemId(MpiInterface::GetSystemId())
{
}

void
RingSwitchScheduler::ScheduleSwitches(double endTime)
{
    if (m_systemId == 0)
    {
        std::printf("[RingSwitchScheduler] %u orbits x %u satellites, safety margin %.1f s\n",
                    m_numOrbits, m_satsPerOrbit, m_config.safetyMarginS);
    }
    ScheduleRing(Ring::UP, endTime);
    ScheduleRing(Ring::DOWN, endTime);
}

void
RingSwitchScheduler::ScheduleRing(Ring ring, double endTime)
{
    const char* name = (ring == Ring::UP) ? "ring-UP" : "ring-DOWN";
    uint32_t seamRight = (ring == Ring::UP) ? m_config.seamRightUp : m_config.seamRightDown;
    uint32_t rowStart = (ring == Ring::UP) ? m_config.seamLeftUp : m_config.seamLeftDown;
    uint32_t step = 1;
    for (double t = kScanStart; t < endTime; t += kScanStep)
    {
        if (!IsSwitchDue(rowStart, t))
        {
            continue;
        }
        const double fireTime = t + m_config.safetyMarginS;
        if (m_systemId == 0)
        {
            std::printf("[RingSwitchScheduler] %s step %2u: sat%u->sat%u  t*=%.1fs\n",
                        name, step, seamRight, rowStart, fireTime);
        }
        seamRight = GetDownNeighbour(seamRight);
        rowStart = GetDownNeighbour(rowStart);
        Simulator::Schedule(Seconds(fireTime), &RingSwitchScheduler::FireSwitch, this, ring, seamRight, rowStart, step);
        for (SatelliteForwardingApp* app : m_apps)
        {
            if (app)
            {
                app->AddRingSwitchTime(ring == Ring::UP ? UP : DOWN, fireTime);
            }
        }
        step++;
    }
}

bool
RingSwitchScheduler::IsSwitchDue(uint32_t rowStart, double t) const
{
    // Walker-Star: the ISL across the seam is not part of the ring.
    const uint32_t numLinks = m_config.closedRing ? m_numOrbits : m_numOrbits - 1;
    uint32_t current = rowStart;
    uint32_t below = GetDownNeighbour(rowStart);
    for (uint32_t i = 0; i < numLinks; i++)
    {
        if (GetRightIslLength(current, t) >= GetRightIslLength(below, t))
        {
            return false;
        }
        current = m_rightPartner[current];
        below = m_rightPartner[below];
    }
    return true;
}

double
RingSwitchScheduler::GetRightIslLength(uint32_t sat, double t) const
{
    return CalculateDistance(GetPosition(m_nodes.Get(sat), t), GetPosition(m_nodes.Get(m_rightPartner[sat]), t));
}

uint32_t
RingSwitchScheduler::GetDownNeighbour(uint32_t sat) const
{
    const uint32_t orbit = sat / m_satsPerOrbit;
    const uint32_t position = sat % m_satsPerOrbit;
    return orbit * m_satsPerOrbit + (position + m_satsPerOrbit - 1) % m_satsPerOrbit;
}

void
RingSwitchScheduler::FireSwitch(Ring ring, uint32_t initiator, uint32_t rowStart, uint32_t step)
{
    SatelliteForwardingApp* app = (initiator < m_apps.size()) ? m_apps[initiator] : nullptr;
    if (!app)
    {
        return; // the initiator runs on another MPI rank
    }
    std::printf("[RingSwitchScheduler] rank%u t=%.3fs FIRE %s step%u sat%u->sat%u\n",
                m_systemId,
                Simulator::Now().GetSeconds(),
                ring == Ring::UP ? "ring-UP " : "ring-DOWN",
                step,
                initiator,
                rowStart);
    if (ring == Ring::UP)
    {
        app->TriggerRingUpSwitch();
    }
    else
    {
        app->TriggerRingDownSwitch();
    }
}

} // namespace ns3
