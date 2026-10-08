#ifndef SATELLITE_STRATEGY_INTERFACES_H
#define SATELLITE_STRATEGY_INTERFACES_H

#include "ns3/address.h"
#include "ns3/net-device.h"
#include "ns3/packet.h"
#include "ns3/ptr.h"

#include <cstdint>
#include <vector>

namespace ns3
{

class SatelliteForwardingApp;

/// Packet type, carried in SatPacketHeader::GetDirection().
enum direction_t
{
    UP = 0,                ///< stored copy travelling upwards in its orbit
    DOWN = 1,              ///< stored copy travelling downwards in its orbit
    BROADCAST = 2,         ///< broadcast packet
    DUMMY = 3,             ///< dummy packet: only adds queueing delay, dropped by the next satellite
    BROADCAST_CONTROL = 4, ///< control packet of the broadcast algorithm (e.g. a prune)
};

/**
 * Routing of one satellite: forwards every packet the satellite receives.
 */
class RoutingStrategy
{
  public:
    virtual ~RoutingStrategy() = default;

    virtual void Init(SatelliteForwardingApp* app) { m_app = app; }

    /// Receive callback of the four ISL devices.
    virtual bool OnReceive(Ptr<NetDevice> device,
                           Ptr<const Packet> packet,
                           uint16_t protocol,
                           const Address& sender) = 0;

    /// Devices on which this satellite sends a new broadcast of its own.
    virtual std::vector<Ptr<NetDevice>> GetBroadcastFirstHops() const = 0;

    /// Called on the satellite that starts a ring switch.
    virtual void InitiateRingUpSwitch() = 0;
    virtual void InitiateRingDownSwitch() = 0;

  protected:
    SatelliteForwardingApp* m_app{nullptr};
};

/**
 * Content of one satellite: generates the objects it stores or broadcasts.
 */
class ContentStrategy
{
  public:
    virtual ~ContentStrategy() = default;

    virtual void Init(SatelliteForwardingApp* app) { m_app = app; }

    /// Starts the generation (called once when the application starts).
    virtual void Generate() = 0;

  protected:
    SatelliteForwardingApp* m_app{nullptr};
};

} // namespace ns3

#endif // SATELLITE_STRATEGY_INTERFACES_H
