// Storage ring routing for Walker-Star constellations.
//
// Topology
//   devUp / devDown connect a satellite to the next / previous satellite of its
//   orbit, devRight / devLeft to the satellite of the next / previous orbit.
//   The seam lies between the seam-right orbit and the seam-left orbit, whose
//   satellites move in opposite directions (IsSeamRight / IsSeamLeft). The
//   ISLs across the seam are never used. From left to right the orbits are
//   seam-left orbit, the orbits in between, seam-right orbit.
//
// Rings (RouteUpRing / RouteDownRing / RouteUpNormal / RouteDownNormal)
//   Every object is stored as an UP copy, which travels upwards in its orbit,
//   and a DOWN copy, which travels downwards. Each ring has one ring satellite
//   per orbit. A copy enters an orbit at the ring satellite, goes once around
//   the orbit (one lap) and leaves it at the ring satellite again, towards the
//   next orbit (UP ring: to the right, DOWN ring: to the left). At the end of
//   the row (the orbit next to the seam) the copies return along the ring row
//   to its start. All other satellites only pass the copies on.
//
// Ring switches (Initiate* / ReceiveUp / ReceiveDown)
//   Cross-plane ISLs cannot be used close to the poles, so both ring rows move
//   by one position at every switch. The switch is carried by the copies
//   themselves: every ring satellite stamps the copies it forwards with its
//   epoch (number of switches so far). The scheduler starts a switch on the
//   new seam-right ring satellite, which takes the next epoch. A satellite
//   that sees a newer epoch from the right becomes a ring satellite (new row),
//   a ring satellite that sees a newer epoch along its orbit retires (old
//   row). The UP row moves against the travel direction of the UP copies, so
//   copies in flight make one hop less in one orbit, once. The DOWN row moves
//   with the travel direction of the DOWN copies: copies in flight leave one
//   orbit after one hop instead of a full lap, once, and the seam-right orbit
//   gets no new input until the new return stream arrives, so its down
//   queues drain.
//
// Dummy packets
//   A new ring satellite fills its new cross-plane queue with dummies, so that
//   the new path has the same queueing delay as the old one. Every satellite
//   tops its storage queues up to their levels before it enqueues a copy
//   (SatelliteForwardingApp::RegulateStorageQueue), and a ring satellite
//   between the seam orbits keeps its left queue at the ISL queue size
//   (ForwardDown). This restores the drained down queues of the seam-right
//   orbit after a DOWN switch and keeps the queues of all orbits equal from
//   switch to switch.
//
// Retired ring satellites (RouteUpNormal / RouteDownNormal)
//   A satellite that is no longer a ring satellite can still receive copies on
//   its cross-plane ports for a short time. It forwards them as the ring would
//   have and never over the seam.
//
// Stored copies (ReceiveUp / ReceiveDown)
//   A copy that was inserted alone carries a flag; satellites that forward it
//   create the missing copy in the other direction while their queue is still
//   filling (SatelliteForwardingApp::CreateSecondCopy). A copy whose time to
//   live has expired is replaced or deleted by its origin, when it passes
//   there (SatelliteForwardingApp::ReplaceExpiredOwnCopy).
//
// Broadcasts (ReceiveBroadcast / GetBroadcastFirstHops)
//   Flooding to all neighbours with duplicate suppression; no cross-plane hops
//   at high latitudes (kBroadcastLatitudeLimit) and none across the seam
//   (IsBroadcastLink).

#include "routing-ring-switch-walker-star.h"

#include "../satellite-forwarding-app.h"

#include "ns3/mobility-model.h"
#include "ns3/node.h"
#include "ns3/simulator.h"

#include <cmath>
#include <cstdio>

namespace ns3
{

namespace
{

/// Top-ups of at least this many dummies are printed.
constexpr uint64_t kTopUpLogThreshold = 100;
/// Broadcasts: no cross-plane hop above this latitude [rad].
constexpr double kBroadcastLatitudeLimit = 60.0 * M_PI / 180.0;

/// Codes in the log of unroutable packets.
constexpr int kUnroutableDownNormal = 0;
constexpr int kUnroutableDownSeamRight = 1;
constexpr int kUnroutableDownSeamLeft = 2;
constexpr int kUnroutableUp = 3;
constexpr int kUnroutableUpSeamLeft = 4;

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Ring roles and switches
// ─────────────────────────────────────────────────────────────────────────────

RoutingRingSwitchStar::RingRole
RoutingRingSwitchStar::GetUpRole() const
{
    if (!m_app->IsRingUp())
    {
        return RingRole::NONE;
    }
    if (m_app->IsSeamLeft())
    {
        return RingRole::SEAM_LEFT;
    }
    return m_app->IsSeamRight() ? RingRole::SEAM_RIGHT : RingRole::NORMAL;
}

RoutingRingSwitchStar::RingRole
RoutingRingSwitchStar::GetDownRole() const
{
    if (!m_app->IsRingDown())
    {
        return RingRole::NONE;
    }
    if (m_app->IsSeamLeft())
    {
        return RingRole::SEAM_LEFT;
    }
    return m_app->IsSeamRight() ? RingRole::SEAM_RIGHT : RingRole::NORMAL;
}

void
RoutingRingSwitchStar::InitiateRingUpSwitch()
{
    // The new seam-right ring satellite sends the lap traffic arriving from
    // below to the left from now on, stamped with the new epoch.
    m_maxUpEpoch++;
    m_app->SetRingUp(true);
    std::printf("[RingSwitchStar] t=%.3fs  sat%u: INITIATING ring-UP switch (epoch %u)\n",
                Simulator::Now().GetSeconds(), m_app->GetSatId(), m_maxUpEpoch);
}

void
RoutingRingSwitchStar::InitiateRingDownSwitch()
{
    // The new seam-right ring satellite sends everything arriving from above
    // to the left from now on, stamped with the new epoch.
    m_maxDownEpoch++;
    m_app->SetRingDown(true);
    std::printf("[RingSwitchStar] t=%.3fs  sat%u: INITIATING ring-DOWN switch (epoch %u)\n",
                Simulator::Now().GetSeconds(), m_app->GetSatId(), m_maxDownEpoch);
}

// ─────────────────────────────────────────────────────────────────────────────
// Routing tables. `device` is the device the copy arrived on: a copy that
// arrives on devUp was sent downwards, one that arrives on devDown upwards.
// ─────────────────────────────────────────────────────────────────────────────

Ptr<NetDevice>
RoutingRingSwitchStar::RouteUpRing(RingRole role, Ptr<NetDevice> device) const
{
    SatelliteForwardingApp* app = m_app;
    switch (role)
    {
    case RingRole::SEAM_LEFT: // start of the forward direction, end of the return stream
        if (device == app->GetDevRight())
        {
            return app->GetDevUp(); // return stream ends: enter this orbit
        }
        if (device == app->GetDevDown())
        {
            return app->GetDevRight(); // lap done: to the next orbit
        }
        break;
    case RingRole::SEAM_RIGHT: // end of the forward direction, start of the return stream
        if (device == app->GetDevLeft())
        {
            return app->GetDevUp(); // enter this orbit
        }
        if (device == app->GetDevDown())
        {
            return app->GetDevLeft(); // lap done: start of the return stream
        }
        break;
    case RingRole::NORMAL:
    case RingRole::NONE:
        if (device == app->GetDevLeft())
        {
            return app->GetDevUp(); // enter this orbit
        }
        if (device == app->GetDevDown())
        {
            return app->GetDevRight(); // lap done: to the next orbit
        }
        if (device == app->GetDevRight())
        {
            return app->GetDevLeft(); // return stream passes through
        }
        break;
    }
    if (device == app->GetDevUp())
    {
        return app->GetDevDown(); // reverse direction (not used by UP copies)
    }
    return nullptr;
}

Ptr<NetDevice>
RoutingRingSwitchStar::RouteDownRing(RingRole role, Ptr<NetDevice> device) const
{
    SatelliteForwardingApp* app = m_app;
    switch (role)
    {
    case RingRole::SEAM_LEFT: // end of the forward direction, start of the return stream
        if (device == app->GetDevRight())
        {
            return app->GetDevDown(); // enter this orbit
        }
        if (device == app->GetDevUp())
        {
            return app->GetDevRight(); // lap done: start of the return stream
        }
        break;
    case RingRole::SEAM_RIGHT: // start of the forward direction, end of the return stream
        if (device == app->GetDevLeft())
        {
            return app->GetDevDown(); // return stream ends: enter this orbit
        }
        if (device == app->GetDevUp())
        {
            return app->GetDevLeft(); // lap done: to the next orbit
        }
        break;
    case RingRole::NORMAL:
    case RingRole::NONE:
        if (device == app->GetDevRight())
        {
            return app->GetDevDown(); // enter this orbit
        }
        if (device == app->GetDevUp())
        {
            return app->GetDevLeft(); // lap done: to the next orbit
        }
        if (device == app->GetDevLeft())
        {
            return app->GetDevRight(); // return stream passes through
        }
        break;
    }
    if (device == app->GetDevDown())
    {
        return app->GetDevUp(); // reverse direction (not used by DOWN copies)
    }
    return nullptr;
}

Ptr<NetDevice>
RoutingRingSwitchStar::RouteUpNormal(Ptr<NetDevice> device) const
{
    SatelliteForwardingApp* app = m_app;
    if (device == app->GetDevDown())
    {
        return app->GetDevUp();
    }
    if (device == app->GetDevUp())
    {
        return app->GetDevDown();
    }
    // late traffic of a retired ring satellite
    if (device == app->GetDevLeft())
    {
        return app->GetDevUp(); // forward stream: enter this orbit
    }
    if (device == app->GetDevRight())
    {
        // return stream: continue along the row; at the end of the row enter the orbit
        return app->IsSeamLeft() ? app->GetDevUp() : app->GetDevLeft();
    }
    return nullptr;
}

Ptr<NetDevice>
RoutingRingSwitchStar::RouteDownNormal(Ptr<NetDevice> device) const
{
    SatelliteForwardingApp* app = m_app;
    if (device == app->GetDevDown())
    {
        return app->GetDevUp();
    }
    if (device == app->GetDevUp())
    {
        return app->GetDevDown();
    }
    // late traffic of a retired ring satellite
    if (device == app->GetDevLeft())
    {
        // return stream: continue along the row; at the end of the row enter the orbit
        return app->IsSeamRight() ? app->GetDevDown() : app->GetDevRight();
    }
    if (device == app->GetDevRight())
    {
        return app->GetDevDown(); // forward stream: enter this orbit
    }
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// Down-queue regulation
// ─────────────────────────────────────────────────────────────────────────────

void
RoutingRingSwitchStar::ForwardDown(Ptr<NetDevice> outDev, Ptr<Packet> packet, RingRole role)
{
    if (outDev == m_app->GetDevDown())
    {
        m_app->RegulateStorageQueue(DOWN);
    }
    else if (outDev == m_app->GetDevLeft() && role == RingRole::NORMAL && m_app->IsLevelActive(DOWN))
    {
        const uint64_t level = m_app->GetIslQueueSize() - 1;
        const uint64_t before = SatelliteForwardingApp::GetQueueOccupancy(outDev);
        if (before < level)
        {
            m_app->FillWithDummies(outDev, level);
            if (level - before >= kTopUpLogThreshold)
            {
                std::printf("[RingSwitchStar] t=%.3fs  sat%u: DOWN-ring left queue topped up %lu -> %lu (dummies)\n",
                            Simulator::Now().GetSeconds(),
                            m_app->GetSatId(),
                            static_cast<unsigned long>(before),
                            static_cast<unsigned long>(SatelliteForwardingApp::GetQueueOccupancy(outDev)));
            }
        }
    }
    m_app->ForwardPacket(outDev, packet);
}

// ─────────────────────────────────────────────────────────────────────────────
// Receiving
// ─────────────────────────────────────────────────────────────────────────────

bool
RoutingRingSwitchStar::OnReceive(Ptr<NetDevice> device,
                                 Ptr<const Packet> packet,
                                 uint16_t protocol,
                                 const Address& sender)
{
    if (protocol != SatelliteForwardingApp::kProtocolData &&
        protocol != SatelliteForwardingApp::kProtocolBroadcast)
    {
        return false;
    }

    Ptr<Packet> copy = packet->Copy();
    SatPacketHeader header;
    copy->RemoveHeader(header);
    const direction_t direction = static_cast<direction_t>(header.GetDirection());

    if (direction == DUMMY)
    {
        return true;
    }
    // Expired broadcasts are dropped; expired stored copies are handled by
    // their origin (SatelliteForwardingApp::ReplaceExpiredOwnCopy).
    const bool expired = SatelliteForwardingApp::IsExpired(header);
    if (expired && direction != UP && direction != DOWN)
    {
        return true;
    }

    // own copy back at its origin
    if (header.GetOrigin() == m_app->GetSatId() && (direction == UP || direction == DOWN) && !expired)
    {
        if (SatelliteForwardingApp::PacketRecord* record = m_app->GetRecord(header.GetId()))
        {
            m_app->RecordReturn(record, direction);
            if (header.GetDupCode() != 0)
            {
                m_app->LogReassembly(header.GetDupCode(), header.GetOrigin(), header.GetId());
            }
        }
    }

    if (direction == DOWN)
    {
        ReceiveDown(device, copy, header);
    }
    else if (direction == UP)
    {
        ReceiveUp(device, copy, header);
    }
    else if (direction == BROADCAST && protocol == SatelliteForwardingApp::kProtocolBroadcast)
    {
        ReceiveBroadcast(device, copy, header);
    }
    return true;
}

void
RoutingRingSwitchStar::ReceiveUp(Ptr<NetDevice> device, Ptr<Packet> packet, SatPacketHeader& header)
{
    const uint32_t lastHop = header.GetLastHop();
    const RingRole role = GetUpRole();
    if (header.GetEpoch() > m_maxUpEpoch)
    {
        m_maxUpEpoch = header.GetEpoch();
        if (role == RingRole::NONE && device == m_app->GetDevRight())
        {
            // newer epoch on the new return row: become a ring satellite; the
            // new cross-plane path gets the queueing delay of the old one
            m_app->SetRingUp(true);
            std::printf("[RingSwitchStar] t=%.3fs  sat%u: activated as new ring-UP (epoch %u)\n",
                        Simulator::Now().GetSeconds(), m_app->GetSatId(), m_maxUpEpoch);
            const uint64_t upQueue = SatelliteForwardingApp::GetQueueOccupancy(m_app->GetDevUp());
            if (upQueue > 0)
            {
                m_app->FillWithDummies(m_app->GetDevRight(), upQueue - 1);
            }
        }
        else if (role != RingRole::NONE && device == m_app->GetDevDown())
        {
            // newer epoch from below (copy that entered at the new ring satellite): retire
            m_app->SetRingUp(false);
            std::printf("[RingSwitchStar] t=%.3fs  sat%u: ring-UP retired%s\n",
                        Simulator::Now().GetSeconds(),
                        m_app->GetSatId(),
                        m_app->IsSeamLeft() ? " (ring-UP switch complete)" : "");
        }
    }
    m_app->SetInsertionEpoch(UP, m_maxUpEpoch);

    if (m_app->ReplaceExpiredOwnCopy(header))
    {
        return;
    }
    if (header.GetDupCode() > 0 && m_app->CreateSecondCopy(DOWN, header, m_maxDownEpoch))
    {
        header.SetDupCode(0);
    }

    const RingRole newRole = GetUpRole();
    if (newRole != RingRole::NONE)
    {
        header.SetEpoch(m_maxUpEpoch);
    }
    header.SetLastHop(m_app->GetSatId());
    packet->AddHeader(header);

    Ptr<NetDevice> out = (newRole != RingRole::NONE) ? RouteUpRing(newRole, device) : RouteUpNormal(device);
    if (!out)
    {
        m_app->LogUnroutable(newRole == RingRole::SEAM_LEFT ? kUnroutableUpSeamLeft : kUnroutableUp, lastHop);
        return;
    }
    if (out == m_app->GetDevUp())
    {
        m_app->RegulateStorageQueue(UP);
    }
    m_app->ForwardPacket(out, packet);
}

void
RoutingRingSwitchStar::ReceiveDown(Ptr<NetDevice> device, Ptr<Packet> packet, SatPacketHeader& header)
{
    const uint32_t lastHop = header.GetLastHop();
    const RingRole role = GetDownRole();
    if (header.GetEpoch() > m_maxDownEpoch)
    {
        m_maxDownEpoch = header.GetEpoch();
        if (role == RingRole::NONE && device == m_app->GetDevRight())
        {
            // newer epoch on the new forward row: become a ring satellite; the
            // new cross-plane path gets the queueing delay of the old one
            m_app->SetRingDown(true);
            std::printf("[RingSwitchStar] t=%.3fs  sat%u: activated as new ring-DOWN (epoch %u)\n",
                        Simulator::Now().GetSeconds(), m_app->GetSatId(), m_maxDownEpoch);
            m_app->FillWithDummies(m_app->GetDevLeft(), m_app->GetIslQueueSize() - 1);
        }
        else if (role != RingRole::NONE && device == m_app->GetDevUp())
        {
            // newer epoch from above (copy that did its lap from the new ring satellite): retire
            m_app->SetRingDown(false);
            std::printf("[RingSwitchStar] t=%.3fs  sat%u: ring-DOWN retired%s\n",
                        Simulator::Now().GetSeconds(),
                        m_app->GetSatId(),
                        m_app->IsSeamLeft() ? " (ring-DOWN switch complete)" : "");
        }
    }
    m_app->SetInsertionEpoch(DOWN, header.GetEpoch());

    if (m_app->ReplaceExpiredOwnCopy(header))
    {
        return;
    }
    if (header.GetDupCode() > 0 && m_app->CreateSecondCopy(UP, header, m_maxUpEpoch))
    {
        header.SetDupCode(0);
    }

    const RingRole newRole = GetDownRole();
    if (newRole != RingRole::NONE)
    {
        header.SetEpoch(m_maxDownEpoch);
    }
    header.SetLastHop(m_app->GetSatId());
    packet->AddHeader(header);

    Ptr<NetDevice> out =
        (newRole != RingRole::NONE) ? RouteDownRing(newRole, device) : RouteDownNormal(device);
    if (!out)
    {
        const int code = (newRole == RingRole::SEAM_LEFT)    ? kUnroutableDownSeamLeft
                         : (newRole == RingRole::SEAM_RIGHT) ? kUnroutableDownSeamRight
                                                             : kUnroutableDownNormal;
        m_app->LogUnroutable(code, lastHop);
        return;
    }
    ForwardDown(out, packet, newRole);
}

// ─────────────────────────────────────────────────────────────────────────────
// Broadcasts
// ─────────────────────────────────────────────────────────────────────────────

bool
RoutingRingSwitchStar::IsBroadcastLink(Ptr<NetDevice> device) const
{
    if ((device == m_app->GetDevLeft() && m_app->IsSeamLeft()) ||
        (device == m_app->GetDevRight() && m_app->IsSeamRight()))
    {
        return false;
    }
    if (device != m_app->GetDevLeft() && device != m_app->GetDevRight())
    {
        return true;
    }
    const Vector pos = m_app->GetNode()->GetObject<MobilityModel>()->GetPosition();
    const double latitude = std::asin(pos.z / std::sqrt(pos.x * pos.x + pos.y * pos.y + pos.z * pos.z));
    return std::abs(latitude) <= kBroadcastLatitudeLimit;
}

std::vector<Ptr<NetDevice>>
RoutingRingSwitchStar::GetBroadcastFirstHops() const
{
    std::vector<Ptr<NetDevice>> hops;
    for (Ptr<NetDevice> device :
         {m_app->GetDevUp(), m_app->GetDevDown(), m_app->GetDevLeft(), m_app->GetDevRight()})
    {
        if (IsBroadcastLink(device))
        {
            hops.push_back(device);
        }
    }
    return hops;
}

void
RoutingRingSwitchStar::ReceiveBroadcast(Ptr<NetDevice> device, Ptr<Packet> packet, SatPacketHeader& header)
{
    const uint16_t origin = header.GetOrigin();
    if (origin == m_app->GetSatId() || m_broadcastFilter.SeenBefore(origin, header.GetId()))
    {
        return;
    }
    if (header.GetDupCode() > 0)
    {
        m_app->LogBroadcast(false, origin, header.GetId());
    }
    header.SetLastHop(m_app->GetSatId());
    packet->AddHeader(header);
    for (Ptr<NetDevice> out :
         {m_app->GetDevUp(), m_app->GetDevDown(), m_app->GetDevLeft(), m_app->GetDevRight()})
    {
        if (out != device && IsBroadcastLink(out))
        {
            m_app->ForwardPacket(out, packet->Copy(), SatelliteForwardingApp::kProtocolBroadcast);
        }
    }
}

} // namespace ns3
