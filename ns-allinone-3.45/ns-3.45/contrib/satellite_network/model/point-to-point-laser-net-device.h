#ifndef POINT_TO_POINT_LASER_NET_DEVICE_H
#define POINT_TO_POINT_LASER_NET_DEVICE_H

#include <cstring>
#include <cstdint>
#include "ns3/address.h"
#include "ns3/node.h"
#include "ns3/net-device.h"
#include "ns3/callback.h"
#include "ns3/packet.h"
#include "ns3/traced-callback.h"
#include "ns3/nstime.h"
#include "ns3/event-id.h"
#include "ns3/data-rate.h"
#include "ns3/ptr.h"
#include "ns3/mac48-address.h"
#include "ns3/drop-tail-queue.h"

#ifdef NS3_MPI
#include "ns3/mpi-interface.h"
#endif

namespace ns3 {

class PointToPointLaserChannel;
class ErrorModel;

/**
 * \ingroup point-to-point
 * \class PointToPointLaserNetDevice
 * \brief A Device for a Point to Point Laser Network Link.
 *
 * This PointToPointLaserNetDevice class specializes the NetDevice abstract
 * base class.  Together with a PointToPointLaserChannel (and a peer
 * PointToPointLaserNetDevice), the class models, with some level of
 * abstraction, a generic point-to-point-laser or serial link.
 *
 * === Traffic shaping ===
 *
 * The device owns TWO transmit queues, selected by the protocol number passed
 * to Send():
 *
 *   PROT_TYPE_A (0x0800) -> m_queue            ("normal")
 *   PROT_TYPE_B (0x0801) -> m_queue_broadcast  ("broadcast")
 *
 * Transmission is organised as a fixed TDMA slot grid anchored at simulation
 * time zero.  One frame is sent per slot, and a slot lasts the transmission
 * time of a maximum-size frame (Mtu + PPP header) plus the interframe gap.
 * Slot n belongs to the normal class iff
 *
 *   (n % (TrafficShare + TrafficShareBroadcast)) < TrafficShare
 *
 * Because the slot length is sized for the worst-case frame, a class holding
 * k out of every P slots can never transmit more than k/P of the line rate
 * over any observation window, regardless of the actual packet sizes.  The
 * burst tolerance is k frames (slots are granted in contiguous blocks).
 *
 * The scheduler is arithmetic, not periodic: no event exists unless a packet
 * is actually going to be transmitted.  A saturated link costs exactly one
 * event per packet (the same as the stock point-to-point device); a gap costs
 * one event regardless of its length; an idle link costs nothing.
 *
 * IMPORTANT: call Time::SetResolution (Time::PS) at the very start of the simulation
 * scenario.  At 80 Gbps a 1502-byte slot is 150.2 ns, which is not
 * representable in nanoseconds, and the rounding error will potentially accumulate in
 * the slot grid.
 */
class PointToPointLaserNetDevice : public NetDevice
{
public:
  /**
   * \brief Get the TypeId
   *
   * \return The TypeId for this class
   */
  static TypeId GetTypeId (void);

  /**
   * Construct a PointToPointLaserNetDevice
   *
   * This is the constructor for the PointToPointLaserNetDevice
   */
  PointToPointLaserNetDevice ();

  /**
   * Destroy a PointToPointLaserNetDevice
   *
   * This is the destructor for the PointToPointLaserNetDevice.
   */
  virtual ~PointToPointLaserNetDevice ();

  /**
   * Protocol number selecting the normal (class A) transmit queue.
   */
  static const uint16_t PROT_TYPE_A = 0x0800;

  /**
   * Protocol number selecting the broadcast (class B) transmit queue.
   */
  static const uint16_t PROT_TYPE_B = 0x0801;

  /**
   * Set the Data Rate used for transmission of packets.  The data rate is
   * set in the Attach () method from the corresponding field in the channel
   * to which the device is attached.  It can be overridden using this method.
   *
   * \param bps the data rate at which this object operates
   */
  void SetDataRate (DataRate bps);

  /**
   * \return the data rate at which this object operates
   */
  DataRate GetDataRate (void) const;

  /**
   * Set the interframe gap used to separate packets.  The interframe gap
   * defines the minimum space required between packets sent by this device.
   *
   * \param t the interframe gap time
   */
  void SetInterframeGap (Time t);

  /**
   * \return the interframe gap used to separate packets
   */
  Time GetInterframeGap (void) const;

  /**
   * Attach the device to a channel.
   *
   * \param ch Ptr to the channel to which this object is being attached.
   * \return true if the operation was successful (always true actually)
   */
  bool Attach (Ptr<PointToPointLaserChannel> ch);

  /**
   * Attach the normal (class A) queue to the PointToPointLaserNetDevice.
   *
   * The PointToPointLaserNetDevice "owns" a queue that implements a queueing
   * method such as DropTailQueue or RedQueue
   *
   * \param queue Ptr to the new queue.
   */
  void SetQueue (Ptr<Queue<Packet> > queue);

  /**
   * Get a copy of the attached normal (class A) queue.
   *
   * \returns Ptr to the queue.
   */
  Ptr<Queue<Packet> > GetQueue (void) const;

  /**
   * Attach the broadcast (class B) queue to the PointToPointLaserNetDevice.
   *
   * \param queue Ptr to the new queue.
   */
  void SetBroadcastQueue (Ptr<Queue<Packet> > queue);

  /**
   * Get a copy of the attached broadcast (class B) queue.
   *
   * \returns Ptr to the queue.
   */
  Ptr<Queue<Packet> > GetBroadcastQueue (void) const;

  /**
   * Attach a receive ErrorModel to the PointToPointLaserNetDevice.
   *
   * The PointToPointNetLaserDevice may optionally include an ErrorModel in
   * the packet receive chain.
   *
   * \param em Ptr to the ErrorModel.
   */
  void SetReceiveErrorModel (Ptr<ErrorModel> em);

  /**
   * Receive a packet from a connected PointToPointLaserChannel.
   *
   * The PointToPointLaserNetDevice receives packets from its connected channel
   * and forwards them up the protocol stack.  This is the public method
   * used by the channel to indicate that the last bit of a packet has
   * arrived at the device.
   *
   * \param p Ptr to the received packet.
   */
  void Receive (Ptr<Packet> p);

  // The remaining methods are documented in ns3::NetDevice*

  virtual void SetIfIndex (const uint32_t index);
  virtual uint32_t GetIfIndex (void) const;

  virtual Ptr<Channel> GetChannel (void) const;

  virtual void SetAddress (Address address);
  virtual Address GetAddress (void) const;

  virtual void SetDestinationNode (Ptr<Node> node);
  virtual Ptr<Node> GetDestinationNode (void) const;

  virtual bool SetMtu (const uint16_t mtu);
  virtual uint16_t GetMtu (void) const;

  virtual bool IsLinkUp (void) const;

  virtual void AddLinkChangeCallback (Callback<void> callback);

  virtual bool IsBroadcast (void) const;
  virtual Address GetBroadcast (void) const;

  virtual bool IsMulticast (void) const;
  virtual Address GetMulticast (Ipv4Address multicastGroup) const;

  virtual bool IsPointToPoint (void) const;
  virtual bool IsBridge (void) const;

  virtual bool Send (Ptr<Packet> packet, const Address &dest, uint16_t protocolNumber);
  virtual bool SendFrom (Ptr<Packet> packet, const Address& source, const Address& dest, uint16_t protocolNumber);

  virtual Ptr<Node> GetNode (void) const;
  virtual void SetNode (Ptr<Node> node);

  virtual bool NeedsArp (void) const;

  virtual void SetReceiveCallback (NetDevice::ReceiveCallback cb);

  virtual Address GetMulticast (Ipv6Address addr) const;

  virtual void SetPromiscReceiveCallback (PromiscReceiveCallback cb);
  virtual bool SupportsSendFrom (void) const;

protected:
  /**
   * \brief Handler for MPI receive event
   *
   * \param p Packet received
   */
  void DoMpiReceive (Ptr<Packet> p);

private:

  /**
   * \brief Assign operator
   *
   * The method is private, so it is DISABLED.
   *
   * \param o Other NetDevice
   * \return New instance of the NetDevice
   */
  PointToPointLaserNetDevice& operator = (const PointToPointLaserNetDevice &o);

  /**
   * \brief Copy constructor
   *
   * The method is private, so it is DISABLED.

   * \param o Other NetDevice
   */
  PointToPointLaserNetDevice (const PointToPointLaserNetDevice &o);

  /**
   * \brief Dispose of the object
   */
  virtual void DoDispose (void);

private:

  /**
   * \returns the address of the remote device connected to this device
   * through the point to point channel.
   */
  Address GetRemote (void) const;

  /**
   * Adds the necessary headers and trailers to a packet of data in order to
   * respect the protocol implemented by the agent.
   * \param p packet
   * \param protocolNumber protocol number
   */
  void AddHeader (Ptr<Packet> p, uint16_t protocolNumber);

  /**
   * Removes, from a packet of data, all headers and trailers that
   * relate to the protocol implemented by the agent
   * \param p Packet whose headers need to be processed
   * \param param An integer parameter that can be set by the function
   * \return Returns true if the packet should be forwarded up the
   * protocol stack.
   */
  bool ProcessHeader (Ptr<Packet> p, uint16_t& param);

  /**
   * Start Sending a Packet Down the Wire.
   *
   * The TransmitStart method is the method that is used internally in the
   * PointToPointLaserNetDevice to begin the process of sending a packet out on
   * the channel.  The corresponding method is called on the channel to let
   * it know that the physical device this class represents has virtually
   * started sending signals.  An event is scheduled for the END OF THE SLOT,
   * not for the end of the frame: the transmitter is committed for the whole
   * slot, which is what bounds the per-class rate independently of the packet
   * size.  The channel is still told the true on-air time.
   *
   * \see PointToPointLaserChannel::TransmitStart ()
   * \see TransmitComplete()
   * \param p a reference to the packet to send
   * \returns true if success, false on failure
   */
  bool TransmitStart (Ptr<Packet> p);

  /**
   * Stop Sending a Packet Down the Wire and Begin the Interframe Gap.
   *
   * The TransmitComplete method is used internally to finish the process
   * of sending a packet out on the channel.  It fires exactly on a slot
   * boundary and hands control back to the shaper.
   */
  void TransmitComplete (void);

  /**
   * \brief Recompute the shaper's next action and (re)arm the slot event.
   *
   * Cheap and idempotent; safe to call after any enqueue or transmission.
   * Does nothing while a transmission is in flight, since TransmitComplete()
   * re-arms at the following slot boundary anyway.
   */
  void ArmShaper (void);

  /**
   * \brief Slot event handler: serve the slot the event was armed for.
   */
  void SlotTick (void);

  /**
   * \brief Dequeue from the class owning slot m_nextSlot and start its
   *        transmission.  Must only be called exactly on a slot boundary,
   *        with that class non-empty.
   */
  void StartSlot (void);

  /**
   * \brief Recompute m_slotTime from the MTU, data rate and interframe gap.
   */
  void UpdateSlotTime (void);

  /**
   * \param n slot index
   * \return true if slot n belongs to the normal (class A) queue
   */
  bool IsNormalSlot (uint64_t n) const;

  /**
   * \param from first slot index that may be used
   * \param normalClass true for the normal queue, false for the broadcast queue
   * \return the first slot at or after \p from that belongs to that class
   */
  uint64_t NextSlotForClass (uint64_t from, bool normalClass) const;

  /**
   * \brief Make the link up and running
   *
   * It calls also the linkChange callback.
   */
  void NotifyLinkUp (void);

  /**
   * Enumeration of the states of the transmit machine of the net device.
   */
  enum TxMachineState
  {
    READY,   /**< The transmitter is ready to begin transmission of a packet */
    BUSY     /**< The transmitter is busy transmitting a packet */
  };
  /**
   * The state of the Net Device transmit state machine.
   */
  TxMachineState m_txMachineState;

  /**
   * The data rate that the Net Device uses to simulate packet transmission
   * timing.
   */
  DataRate       m_bps;

  /**
   * The interframe gap that the Net Device uses to throttle packet
   * transmission
   */
  Time           m_tInterframeGap;

  /**
   * The PointToPointLaserChannel to which this PointToPointLaserNetDevice
   * has been attached.
   */
  Ptr<PointToPointLaserChannel> m_channel;

  /**
   * The Queues which this PointToPointLaserNetDevice uses as packet sources.
   * Management of these Queues has been delegated to the PointToPointLaserNetDevice
   * and it has the responsibility for deletion.
   * \see class DropTailQueue
   */
  Ptr<Queue<Packet> > m_queue;
  Ptr<Queue<Packet> > m_queue_broadcast;

  /**
   * Error model for receive packet events
   */
  Ptr<ErrorModel> m_receiveErrorModel;

  /**
   * The trace source fired when packets come into the "top" of the device
   * at the L3/L2 transition, before being queued for transmission.
   */
  TracedCallback<Ptr<const Packet> > m_macTxTrace;

  /**
   * The trace source fired when packets coming into the "top" of the device
   * at the L3/L2 transition are dropped before being queued for transmission.
   */
  TracedCallback<Ptr<const Packet> > m_macTxDropTrace;

  /**
   * The trace source fired for packets successfully received by the device
   * immediately before being forwarded up to higher layers (at the L2/L3 
   * transition).  This is a promiscuous trace (which doesn't mean a lot here
   * in the point-to-point-laser device).
   */
  TracedCallback<Ptr<const Packet> > m_macPromiscRxTrace;

  /**
   * The trace source fired for packets successfully received by the device
   * immediately before being forwarded up to higher layers (at the L2/L3 
   * transition).  This is a non-promiscuous trace (which doesn't mean a lot 
   * here in the point-to-point-laser device).
   */
  TracedCallback<Ptr<const Packet> > m_macRxTrace;

  /**
   * The trace source fired for packets successfully received by the device
   * but are dropped before being forwarded up to higher layers (at the L2/L3 
   * transition).
   */
  TracedCallback<Ptr<const Packet> > m_macRxDropTrace;

  /**
   * The trace source fired when a packet begins the transmission process on
   * the medium.
   */
  TracedCallback<Ptr<const Packet> > m_phyTxBeginTrace;

  /**
   * The trace source fired when a packet ends the transmission process on
   * the medium.
   */
  TracedCallback<Ptr<const Packet> > m_phyTxEndTrace;

  /**
   * The trace source fired when the phy layer drops a packet before it tries
   * to transmit it.
   */
  TracedCallback<Ptr<const Packet> > m_phyTxDropTrace;

  /**
   * The trace source fired when a packet begins the reception process from
   * the medium -- when the simulated first bit(s) arrive.
   */
  TracedCallback<Ptr<const Packet> > m_phyRxBeginTrace;

  /**
   * The trace source fired when a packet ends the reception process from
   * the medium.
   */
  TracedCallback<Ptr<const Packet> > m_phyRxEndTrace;

  /**
   * The trace source fired when the phy layer drops a packet it has received.
   * This happens if the receiver is not enabled or the error model is active
   * and indicates that the packet is corrupt.
   */
  TracedCallback<Ptr<const Packet> > m_phyRxDropTrace;

  /**
   * A trace source that emulates a non-promiscuous protocol sniffer connected 
   * to the device.  Unlike your average everyday sniffer, this trace source 
   * will not fire on PACKET_OTHERHOST events.
   *
   * On the transmit size, this trace hook will fire after a packet is dequeued
   * from the device queue for transmission.  In Linux, for example, this would
   * correspond to the point just before a device \c hard_start_xmit where 
   * \c dev_queue_xmit_nit is called to dispatch the packet to the PF_PACKET 
   * ETH_P_ALL handlers.
   *
   * On the receive side, this trace hook will fire when a packet is received,
   * just before the receive callback is executed.  In Linux, for example, 
   * this would correspond to the point at which the packet is dispatched to 
   * packet sniffers in \c netif_receive_skb.
   */
  TracedCallback<Ptr<const Packet> > m_snifferTrace;

  /**
   * A trace source that emulates a promiscuous mode protocol sniffer connected
   * to the device.  This trace source fire on packets destined for any host
   * just like your average everyday packet sniffer.
   *
   * On the transmit size, this trace hook will fire after a packet is dequeued
   * from the device queue for transmission.  In Linux, for example, this would
   * correspond to the point just before a device \c hard_start_xmit where 
   * \c dev_queue_xmit_nit is called to dispatch the packet to the PF_PACKET 
   * ETH_P_ALL handlers.
   *
   * On the receive side, this trace hook will fire when a packet is received,
   * just before the receive callback is executed.  In Linux, for example, 
   * this would correspond to the point at which the packet is dispatched to 
   * packet sniffers in \c netif_receive_skb.
   */
  TracedCallback<Ptr<const Packet> > m_promiscSnifferTrace;

  Ptr<Node> m_node;              //!< Node owning this NetDevice
  Ptr<Node> m_destination_node;  //!< Node at the other end of the p2pLaserLink
  Mac48Address m_address;        //!< Mac48Address of this NetDevice
  NetDevice::ReceiveCallback m_rxCallback;   //!< Receive callback
  NetDevice::PromiscReceiveCallback m_promiscCallback;  //!< Receive callback
                                                        //   (promisc data)
  uint32_t m_ifIndex; //!< Index of the interface
  bool m_linkUp;      //!< Identify if the link is up or not
  TracedCallback<> m_linkChangeCallbacks;  //!< Callback for the link change event

  static const uint16_t DEFAULT_MTU = 1500; //!< Default MTU

  /**
   * \brief Number of bytes the PPP header adds to every frame.
   */
  static const uint32_t PPP_HEADER_BYTES = 2;

  /**
   * \brief The Maximum Transmission Unit
   *
   * This corresponds to the maximum 
   * number of bytes that can be transmitted as seen from higher layers.
   * This corresponds to the 1500 byte MTU size often seen on IP over 
   * Ethernet.
   */
  uint32_t m_mtu;

  Ptr<Packet> m_currentPkt; //!< Current packet processed

  /**
   * \brief PPP to Ethernet protocol number mapping
   * \param protocol A PPP protocol number
   * \return The corresponding Ethernet protocol number
   */
  static uint16_t PppToEther (uint16_t protocol);

  /**
   * \brief Ethernet to PPP protocol number mapping
   * \param protocol An Ethernet protocol number
   * \return The corresponding PPP protocol number
   */
  static uint16_t EtherToPpp (uint16_t protocol);

private:
  bool m_utilization_tracking_enabled = false;
  int64_t m_interval_ns;
  int64_t m_prev_time_ns;
  int64_t m_current_interval_start;
  int64_t m_current_interval_end;
  int64_t m_idle_time_counter_ns;
  int64_t m_busy_time_counter_ns;
  bool m_current_state_is_on;
  std::vector<double> m_utilization;
  void TrackUtilization(bool next_state_is_on);

  //
  // Shaper state.  The slot grid is anchored at t = 0 and derived purely by
  // arithmetic, so there is no periodic timer and no phase that can drift.
  //
  Time     m_slotTime;    //!< Worst-case frame time + IFG: the grid quantum
  Time     m_txStartTicks;
  uint64_t m_nextSlot;    //!< First slot index not yet consumed
  uint64_t m_armedSlot;   //!< Slot index m_slotEvent will serve
  EventId  m_slotEvent;   //!< Pending wake-up, if any

  uint16_t m_traffic_share {1};            //!< Slots per period for m_queue
  uint16_t m_traffic_share_broadcast {0};  //!< Slots per period for m_queue_broadcast

public:
    void EnableUtilizationTracking(int64_t interval_ns);
    const std::vector<double>& FinalizeUtilization();

};

} // namespace ns3

#endif /* POINT_TO_POINT_LASER_NET_DEVICE_H */