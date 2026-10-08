#ifndef ROUTING_RING_SWITCH_WALKER_DELTA_H
#define ROUTING_RING_SWITCH_WALKER_DELTA_H

#include "../satellite-forwarding-app.h"
#include "../strategy-interfaces.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ns3
{

/**
 * Broadcast algorithm of the Walker-Delta routing (one object per satellite).
 *
 * The routing drops duplicates (BroadcastFilter) and hands every broadcast to
 * the algorithm selected with --broadcastAlgorithm. A new algorithm derives
 * from this class and is added to the list of algorithms at the top of
 * routing-ring-switch-walker-delta.cc.
 */
class DeltaBroadcastAlgorithm
{
  public:
    virtual ~DeltaBroadcastAlgorithm() = default;

    virtual void Init(SatelliteForwardingApp* app) { m_app = app; }
    /// Devices on which this satellite sends a new broadcast of its own
    /// (default: all four ISLs).
    virtual std::vector<Ptr<NetDevice>> GetFirstHops() const;
    /// A broadcast arrived for the first time on `device` (`packet` without header).
    virtual void Forward(Ptr<NetDevice> device, Ptr<Packet> packet, SatPacketHeader header) = 0;
    /// A broadcast that was received before arrived again on `device`.
    virtual void OnDuplicate(Ptr<NetDevice> device, const SatPacketHeader& header) {}
    /// A BROADCAST_CONTROL packet of the algorithm arrived on `device`.
    virtual void OnControl(Ptr<NetDevice> device, const SatPacketHeader& header) {}

  protected:
    /// The four ISL devices: up, down, left, right.
    std::array<Ptr<NetDevice>, 4> GetDevices() const;
    /// Index of `device` in GetDevices() (-1: unknown device).
    int GetPortIndex(Ptr<NetDevice> device) const;
    /// Sends a copy of `packet` with `header` on `device` (broadcast queue).
    void Send(Ptr<NetDevice> device, Ptr<Packet> packet, const SatPacketHeader& header) const;

    SatelliteForwardingApp* m_app{nullptr};
};

/**
 * Storage ring routing with ring switches for Walker-Delta constellations
 * (constellations without a seam, e.g. Starlink).
 *
 * UP copies travel upwards in their orbit, DOWN copies downwards. Every copy
 * goes once around its orbit and leaves it at the exit satellite of the orbit
 * towards the next orbit; the exits of all orbits form a closed ring. The
 * exits move by one position at every ring switch. See the description at the
 * top of the .cc file.
 */
class RoutingRingSwitchDelta : public RoutingStrategy
{
  public:
    /// Names of the broadcast algorithms; the first one is the default.
    static std::vector<std::string> GetBroadcastAlgorithmNames();

    /// `broadcastAlgorithm`: one of GetBroadcastAlgorithmNames() (empty: default).
    explicit RoutingRingSwitchDelta(const std::string& broadcastAlgorithm = "");
    ~RoutingRingSwitchDelta() override;

    void Init(SatelliteForwardingApp* app) override;
    bool OnReceive(Ptr<NetDevice> device,
                   Ptr<const Packet> packet,
                   uint16_t protocol,
                   const Address& sender) override;
    std::vector<Ptr<NetDevice>> GetBroadcastFirstHops() const override;
    /// Called on the new exit of the kink orbit when a switch starts.
    void InitiateRingUpSwitch() override;
    void InitiateRingDownSwitch() override;

  private:
    /// True if `sat` is the exit of its orbit in the given epoch.
    bool IsUpExit(uint32_t sat, uint32_t epoch) const;
    bool IsDownExit(uint32_t sat, uint32_t epoch) const;
    /// Takes the role (exit, entry or none) of this satellite in `epoch`.
    void UpdateUpRole(uint32_t epoch, bool announce, bool initiator);
    void UpdateDownRole(uint32_t epoch);

    void ReceiveUp(Ptr<NetDevice> device, Ptr<Packet> packet, SatPacketHeader& header);
    void ReceiveDown(Ptr<NetDevice> device, Ptr<Packet> packet, SatPacketHeader& header);
    void ReceiveBroadcast(Ptr<NetDevice> device, Ptr<Packet> packet, SatPacketHeader& header);

    /// Output device of a copy that arrived on `device` (nullptr: no rule).
    /// `leavesOrbit` is set if the copy leaves the orbit over the ring ISL.
    Ptr<NetDevice> RouteUp(Ptr<NetDevice> device, bool& leavesOrbit) const;
    /// DOWN copies exit at the exit of their own `epoch`, which the switch
    /// initiator raises for copies entering there.
    Ptr<NetDevice> RouteDown(Ptr<NetDevice> device, uint32_t& epoch);

    /// Highest epoch seen per ring (on passing copies or as switch initiator).
    uint32_t m_maxUpEpoch{0};
    uint32_t m_maxDownEpoch{0};

    /// Exit position of every orbit at epoch 0; at epoch e the exit of orbit k
    /// is at position (exit0[k] - e) mod satellitesPerOrbit.
    std::vector<int32_t> m_upExit0;
    std::vector<int32_t> m_downExit0;
    /// Sat ids of the left / right ISL neighbours.
    uint32_t m_leftPeer{UINT32_MAX};
    uint32_t m_rightPeer{UINT32_MAX};

    /// UP exit of epoch m_maxUpEpoch.
    bool m_upExit{false};
    /// DOWN switch: epoch given to copies entering here (initiators only).
    uint32_t m_downInitiatedEpoch{0};
    /// Newest epoch of the DOWN copies this satellite has sent to the next orbit.
    uint32_t m_downExitEpoch{0};

    std::unique_ptr<DeltaBroadcastAlgorithm> m_broadcast;
    BroadcastFilter m_broadcastFilter;
};

} // namespace ns3

#endif // ROUTING_RING_SWITCH_WALKER_DELTA_H
