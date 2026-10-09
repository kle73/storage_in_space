#ifndef CONTENT_FILL_DOUBLE_H
#define CONTENT_FILL_DOUBLE_H

#include "../strategy-interfaces.h"

#include <cstdint>
#include <deque>

namespace ns3
{

class SatPacketHeader;

/**
 * Object generation and insertion.
 *
 * Every satellite generates objects of GetObjectSize() packets at random
 * times; they wait in a pending list until they can be stored. In STORAGE
 * mode every object is stored as two copies, an UP and a DOWN copy:
 *  - inserted into the UP and DOWN queue of the satellite whenever the storage
 *    levels admit it (SatelliteForwardingApp::AdmitObject). While a queue is
 *    still filling (fill period, see SatelliteForwardingApp) and only one of
 *    the two queues has room, that copy is inserted alone and flagged (dup
 *    code 1), and the routing creates the second copy later at a satellite
 *    whose queue is still filling;
 *  - or copy by copy in place of expired copies of this satellite
 *    (TakeReplacementCopy), UP copies in place of UP copies and DOWN copies in
 *    place of DOWN copies.
 * In BROADCAST mode every object is sent to all satellites instead; the
 * routing chooses the first hops and forwards the broadcast.
 *
 * No object is inserted around a ring switch.
 */
class ContentFillDouble : public ContentStrategy
{
  public:
    enum class Mode
    {
        STORAGE,
        BROADCAST
    };

    explicit ContentFillDouble(Mode mode);

    void Init(SatelliteForwardingApp* app) override;
    void Generate() override;

  private:
    struct PendingObject
    {
        uint32_t id;
        uint32_t numPackets;
    };

    /// Object that is stored copy by copy in place of expired copies.
    struct ReplacingObject
    {
        uint32_t id;
        uint32_t numPackets;
        uint32_t firstPacketId;
        uint64_t creationTimeMs;
        uint32_t numStored[2]; ///< copies stored so far, UP and DOWN
    };

    void GenerateObject();
    void SendObject();
    /// Inserts the copies of `object` the storage levels admit; returns false
    /// if the object has to wait.
    bool InsertObject(const PendingObject& object);
    void BroadcastObject(const PendingObject& object);
    /// Writes the next copy of `direction` of the objects being stored in
    /// place of expired copies into `header`; false if none is waiting.
    bool TakeReplacementCopy(direction_t direction, SatPacketHeader& header);
    Ptr<Packet> MakeFragment(uint32_t objectId,
                             uint32_t fragmentId,
                             direction_t direction,
                             uint32_t epoch,
                             uint16_t dupCode,
                             uint32_t id) const;

    Mode m_mode;
    std::deque<PendingObject> m_pending;
    std::deque<ReplacingObject> m_replacing;
    uint64_t m_numPendingPackets{0};
    bool m_generationStopped{false};
};

} // namespace ns3

#endif // CONTENT_FILL_DOUBLE_H
