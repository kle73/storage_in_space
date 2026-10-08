#include "content-fill-double.h"

#include "../satellite-forwarding-app.h"

#include "ns3/point-to-point-laser-net-device.h"
#include "ns3/queue.h"
#include "ns3/simulator.h"

#include <algorithm>
#include <cstddef>
#include <iterator>

namespace ns3
{

namespace
{

/// Objects are generated in random intervals of 2 to 12 us (one try per interval).
constexpr int64_t kGenerationIntervalMinUs = 2;
constexpr uint32_t kGenerationIntervalSpreadUs = 10;
/// Polling intervals of the insertion [us]: next object; retry while the
/// queues are filling; retry while the levels are exhausted or around a ring
/// switch (the levels only change when copies are deleted).
constexpr int64_t kInsertIntervalUs = 10;
constexpr int64_t kRetryIntervalUs = 20;
constexpr int64_t kIdleRetryIntervalUs = 1000;
/// Objects that cannot be stored wait in a list of at most this many objects;
/// further objects are rejected.
constexpr std::size_t kMaxPendingObjects = 1000;
/// Packet ids are 32 bit: stop generating before they run out.
constexpr uint64_t kMaxPacketIds = 4100000000ULL;
/// Broadcasts: fraction (percent) sampled for the delivery statistics, from this time on [s].
constexpr uint32_t kBroadcastSamplePercent = 1;
constexpr double kBroadcastSampleStart = 1.5;

} // namespace

ContentFillDouble::ContentFillDouble(Mode mode)
    : m_mode(mode)
{
}

void
ContentFillDouble::Init(SatelliteForwardingApp* app)
{
    m_app = app;
    if (m_mode == Mode::STORAGE)
    {
        m_app->SetReplacementSource([this](direction_t direction, SatPacketHeader& header) {
            return TakeReplacementCopy(direction, header);
        });
    }
}

void
ContentFillDouble::Generate()
{
    const uint32_t offset = m_app->GetRandom()->GetInteger(0, kGenerationIntervalSpreadUs);
    Simulator::Schedule(MicroSeconds(10 + offset), &ContentFillDouble::GenerateObject, this);
    Simulator::Schedule(MilliSeconds(10), &ContentFillDouble::SendObject, this);
}

void
ContentFillDouble::GenerateObject()
{
    // every try creates an object with probability 1 / number of satellites
    const uint32_t roll = m_app->GetRandom()->GetInteger(0, m_app->GetNumSatellites() - 1);
    if (roll < 1 && m_pending.size() < kMaxPendingObjects)
    {
        m_pending.push_back({m_app->AllocateObjectId(), m_app->GetObjectSize()});
        m_numPendingPackets += m_app->GetObjectSize();
    }

    if (m_app->GetNumAllocatedPacketIds() + m_numPendingPackets < kMaxPacketIds)
    {
        const uint32_t offset = m_app->GetRandom()->GetInteger(0, kGenerationIntervalSpreadUs);
        Simulator::Schedule(MicroSeconds(kGenerationIntervalMinUs + offset),
                            &ContentFillDouble::GenerateObject,
                            this);
    }
    else
    {
        m_generationStopped = true;
    }
}

void
ContentFillDouble::SendObject()
{
    if (m_app->IsInSwitchGuard())
    {
        Simulator::Schedule(MicroSeconds(kIdleRetryIntervalUs), &ContentFillDouble::SendObject, this);
        return;
    }
    if (m_pending.empty())
    {
        if (!m_generationStopped)
        {
            Simulator::Schedule(MicroSeconds(kInsertIntervalUs), &ContentFillDouble::SendObject, this);
        }
        return;
    }

    const PendingObject object = m_pending.front();
    if (m_mode == Mode::BROADCAST)
    {
        BroadcastObject(object);
    }
    else if (!InsertObject(object))
    {
        const bool levelsActive = m_app->IsLevelActive(UP) && m_app->IsLevelActive(DOWN);
        Simulator::Schedule(MicroSeconds(levelsActive ? kIdleRetryIntervalUs : kRetryIntervalUs),
                            &ContentFillDouble::SendObject,
                            this);
        return;
    }
    m_pending.pop_front();
    m_numPendingPackets -= object.numPackets;
    Simulator::Schedule(MicroSeconds(m_mode == Mode::BROADCAST ? kRetryIntervalUs : kInsertIntervalUs),
                        &ContentFillDouble::SendObject,
                        this);
}

bool
ContentFillDouble::InsertObject(const PendingObject& object)
{
    const SatelliteForwardingApp::Admission admission = m_app->AdmitObject(object.numPackets);
    if (admission == SatelliteForwardingApp::Admission::NONE)
    {
        return false;
    }
    const bool up = admission != SatelliteForwardingApp::Admission::DOWN_ONLY;
    const bool down = admission != SatelliteForwardingApp::Admission::UP_ONLY;

    // a single copy is flagged so that the routing creates the second one
    const uint16_t dupCode = (up && down) ? 0 : 1;
    Ptr<NetDevice> devUp = m_app->GetDevUp();
    Ptr<NetDevice> devDown = m_app->GetDevDown();
    for (uint32_t i = 0; i < object.numPackets; i++)
    {
        const uint32_t id = m_app->AllocatePacketId();
        if (up)
        {
            Ptr<Packet> packet = MakeFragment(object.id, i, UP, m_app->GetInsertionEpoch(UP), dupCode, id);
            devUp->Send(packet, devUp->GetBroadcast(), SatelliteForwardingApp::kProtocolData);
        }
        if (down)
        {
            Ptr<Packet> packet =
                MakeFragment(object.id, i, DOWN, m_app->GetInsertionEpoch(DOWN), dupCode, id);
            devDown->Send(packet, devDown->GetBroadcast(), SatelliteForwardingApp::kProtocolData);
        }
    }
    m_app->LogObjectInsertion(object.id, object.numPackets, (up && down) ? 0 : (up ? 1 : 2));
    return true;
}

bool
ContentFillDouble::TakeReplacementCopy(direction_t direction, SatPacketHeader& header)
{
    const int index = (direction == DOWN) ? 1 : 0;
    // the oldest object that still lacks a copy of this direction, or a new one
    auto object = std::find_if(m_replacing.begin(), m_replacing.end(), [index](const ReplacingObject& o) {
        return o.numStored[index] < o.numPackets;
    });
    if (object == m_replacing.end())
    {
        if (m_pending.empty())
        {
            return false;
        }
        const PendingObject pending = m_pending.front();
        m_pending.pop_front();
        m_numPendingPackets -= pending.numPackets;
        ReplacingObject replacing{pending.id,
                                  pending.numPackets,
                                  m_app->GetNumAllocatedPacketIds(),
                                  static_cast<uint64_t>(Simulator::Now().GetMilliSeconds()),
                                  {0, 0}};
        for (uint32_t i = 0; i < pending.numPackets; i++)
        {
            m_app->AllocatePacketId();
        }
        m_app->LogObjectInsertion(pending.id, pending.numPackets, 0);
        m_replacing.push_back(replacing);
        object = std::prev(m_replacing.end());
    }

    const uint32_t fragment = object->numStored[index]++;
    header.SetId(object->firstPacketId + fragment);
    header.SetOrigin(m_app->GetSatId());
    header.SetCreationTimeMs(object->creationTimeMs);
    header.SetTtl(m_app->GetObjectTtl());
    header.SetObjectId(object->id);
    header.SetFragmentId(fragment);
    header.SetDupCode(0);

    while (!m_replacing.empty() && m_replacing.front().numStored[0] == m_replacing.front().numPackets &&
           m_replacing.front().numStored[1] == m_replacing.front().numPackets)
    {
        m_replacing.pop_front();
    }
    return true;
}

void
ContentFillDouble::BroadcastObject(const PendingObject& object)
{
    const uint32_t firstId = m_app->GetNumAllocatedPacketIds();
    for (uint32_t i = 0; i < object.numPackets; i++)
    {
        m_app->AllocatePacketId();
    }

    // A small sample of the broadcasts is logged at every satellite it reaches.
    bool sampled = false;
    if (Simulator::Now().GetSeconds() >= kBroadcastSampleStart)
    {
        sampled = m_app->GetRandom()->GetInteger(0, 100) < kBroadcastSamplePercent;
    }

    // The routing chooses the first hops; an object is only sent on a link
    // whose broadcast queue has room for all of it.
    for (Ptr<NetDevice> device : m_app->GetBroadcastFirstHops())
    {
        Ptr<Queue<Packet>> queue = DynamicCast<PointToPointLaserNetDevice>(device)->GetBroadcastQueue();
        const int64_t space = static_cast<int64_t>(m_app->GetFillLevel()) -
                              static_cast<int64_t>(queue->GetCurrentSize().GetValue());
        if (space <= 0 || static_cast<int64_t>(object.numPackets) > space)
        {
            continue;
        }
        for (uint32_t i = 0; i < object.numPackets; i++)
        {
            Ptr<Packet> packet = MakeFragment(object.id, i, BROADCAST, 0, sampled ? 1 : 0, firstId + i);
            device->Send(packet, device->GetBroadcast(), SatelliteForwardingApp::kProtocolBroadcast);
            if (sampled)
            {
                m_app->LogBroadcast(true, m_app->GetSatId(), firstId + i);
            }
        }
    }
}

Ptr<Packet>
ContentFillDouble::MakeFragment(uint32_t objectId,
                                uint32_t fragmentId,
                                direction_t direction,
                                uint32_t epoch,
                                uint16_t dupCode,
                                uint32_t id) const
{
    Ptr<Packet> packet = Create<Packet>(SatelliteForwardingApp::kPayloadSize);
    SatPacketHeader header;
    header.SetId(id);
    header.SetOrigin(m_app->GetSatId());
    header.SetCreationTimeMs(Simulator::Now().GetMilliSeconds());
    header.SetTtl(m_app->GetObjectTtl());
    header.SetDirection(direction);
    header.SetEpoch(epoch);
    header.SetObjectId(objectId);
    header.SetFragmentId(fragmentId);
    header.SetLastHop(m_app->GetSatId());
    header.SetDupCode(dupCode);
    packet->AddHeader(header);
    return packet;
}

} // namespace ns3
