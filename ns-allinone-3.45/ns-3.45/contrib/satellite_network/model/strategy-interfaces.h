#ifndef SATELLITE_STRATEGY_INTERFACES_H
#define SATELLITE_STRATEGY_INTERFACES_H

#include "ns3/ptr.h"
#include "ns3/net-device.h"
#include "ns3/packet.h"
#include "ns3/address.h"

namespace ns3 {

class SatelliteForwardingApp;

struct RoutingStrategy {
    virtual ~RoutingStrategy() = default;
    virtual void Init (SatelliteForwardingApp* app) { m_app = app; }
    virtual bool OnReceive (Ptr<NetDevice>    device,
                            Ptr<const Packet> packet,
                            uint16_t          protocol,
                            const Address&    sender) = 0;
protected:
    SatelliteForwardingApp* m_app {nullptr};
};

struct ContentStrategy {
    virtual ~ContentStrategy() = default;
    virtual void Init (SatelliteForwardingApp* app) { m_app = app; }
    virtual void Generate () = 0;
protected:
    SatelliteForwardingApp* m_app {nullptr};
};

typedef enum direction {
    UP = 0,
    DOWN = 1,
    BROADCAST = 2,
    DUMMY = 3,
    BROADCAST_PRUNE = 4
} direction_t;

} // namespace ns3
#endif