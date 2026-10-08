#ifndef ROUTING_RING_SWITCH_WALKER_STAR_H
#define ROUTING_RING_SWITCH_WALKER_STAR_H

#include "../satellite-forwarding-app.h"
#include "../strategy-interfaces.h"

#include <cstdint>
#include <vector>

namespace ns3
{

/**
 * Storage ring routing with ring switches for Walker-Star constellations
 * (constellations with a seam, e.g. Iridium).
 *
 * UP copies travel upwards in their orbit, DOWN copies downwards. Each
 * direction has one ring satellite per orbit (the ring row), where a copy
 * that has finished its lap leaves the orbit towards the next orbit. At the
 * seam the copies return along the ring row; the ISLs across the seam are
 * never used. The ring rows are moved by one position at every ring switch.
 * See the description at the top of the .cc file.
 */
class RoutingRingSwitchStar : public RoutingStrategy
{
  public:
    bool OnReceive(Ptr<NetDevice> device,
                   Ptr<const Packet> packet,
                   uint16_t protocol,
                   const Address& sender) override;
    /// All neighbours, except across the seam and over cross-plane ISLs at high latitudes.
    std::vector<Ptr<NetDevice>> GetBroadcastFirstHops() const override;
    /// Called on the new seam-right satellite of the ring when a switch starts.
    void InitiateRingUpSwitch() override;
    void InitiateRingDownSwitch() override;

  private:
    /// Role of this satellite in one of the two rings.
    enum class RingRole
    {
        NONE,       ///< not a ring satellite
        NORMAL,     ///< ring satellite between the seam orbits
        SEAM_LEFT,  ///< ring satellite in the orbit left of the seam
        SEAM_RIGHT, ///< ring satellite in the orbit right of the seam
    };

    RingRole GetUpRole() const;
    RingRole GetDownRole() const;

    void ReceiveUp(Ptr<NetDevice> device, Ptr<Packet> packet, SatPacketHeader& header);
    void ReceiveDown(Ptr<NetDevice> device, Ptr<Packet> packet, SatPacketHeader& header);
    void ReceiveBroadcast(Ptr<NetDevice> device, Ptr<Packet> packet, SatPacketHeader& header);
    /// Broadcasts are not sent across the seam or over cross-plane ISLs at high latitudes.
    bool IsBroadcastLink(Ptr<NetDevice> device) const;

    /// Output device of a copy that arrived on `device` (nullptr: no rule).
    Ptr<NetDevice> RouteUpRing(RingRole role, Ptr<NetDevice> device) const;
    Ptr<NetDevice> RouteDownRing(RingRole role, Ptr<NetDevice> device) const;
    Ptr<NetDevice> RouteUpNormal(Ptr<NetDevice> device) const;
    Ptr<NetDevice> RouteDownNormal(Ptr<NetDevice> device) const;

    /// Sends a DOWN copy; regulated queues are topped up with dummies first.
    void ForwardDown(Ptr<NetDevice> outDev, Ptr<Packet> packet, RingRole role);

    /// Highest epoch seen per ring (on passing copies or as switch initiator).
    uint32_t m_maxUpEpoch{0};
    uint32_t m_maxDownEpoch{0};

    BroadcastFilter m_broadcastFilter;
};

} // namespace ns3

#endif // ROUTING_RING_SWITCH_WALKER_STAR_H
