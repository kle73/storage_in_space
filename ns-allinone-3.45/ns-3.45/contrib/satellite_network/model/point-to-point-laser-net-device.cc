#include "ns3/abort.h"
#include "ns3/log.h"
#include "ns3/queue.h"
#include "ns3/simulator.h"
#include "ns3/mac48-address.h"
#include "ns3/llc-snap-header.h"
#include "ns3/error-model.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/uinteger.h"
#include "ns3/pointer.h"
#include "ns3/ppp-header.h"
#include "point-to-point-laser-net-device.h"
#include "point-to-point-laser-channel.h"

#include <iostream>
#include <stdio.h>
#include <algorithm>
#include <limits>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("PointToPointLaserNetDevice");

NS_OBJECT_ENSURE_REGISTERED (PointToPointLaserNetDevice);

namespace {
/**
 * Sentinel returned by NextSlotForClass() for a class whose share is zero:
 * it owns no slot, ever.
 */
const uint64_t NO_SLOT = std::numeric_limits<uint64_t>::max ();
}

TypeId 
PointToPointLaserNetDevice::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PointToPointLaserNetDevice")
    .SetParent<NetDevice> ()
    .SetGroupName ("PointToPoint")
    .AddConstructor<PointToPointLaserNetDevice> ()
    .AddAttribute ("Mtu", "The MAC-level Maximum Transmission Unit",
                   UintegerValue (DEFAULT_MTU),
                   MakeUintegerAccessor (&PointToPointLaserNetDevice::SetMtu,
                                         &PointToPointLaserNetDevice::GetMtu),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("Address", 
                   "The MAC address of this device.",
                   Mac48AddressValue (Mac48Address ("ff:ff:ff:ff:ff:ff")),
                   MakeMac48AddressAccessor (&PointToPointLaserNetDevice::m_address),
                   MakeMac48AddressChecker ())
    //
    // Routed through the setters so that the cached slot time is recomputed
    // whenever the rate, the gap or the MTU changes.
    //
    .AddAttribute ("DataRate", 
                   "The default data rate for point to point links",
                   DataRateValue (DataRate ("80000000000b/s")),
                   MakeDataRateAccessor (&PointToPointLaserNetDevice::SetDataRate,
                                         &PointToPointLaserNetDevice::GetDataRate),
                   MakeDataRateChecker ())
    .AddAttribute ("ReceiveErrorModel", 
                   "The receiver error model used to simulate packet loss",
                   PointerValue (),
                   MakePointerAccessor (&PointToPointLaserNetDevice::m_receiveErrorModel),
                   MakePointerChecker<ErrorModel> ())
    .AddAttribute ("InterframeGap", 
                   "The time to wait between packet (frame) transmissions",
                   TimeValue (Seconds (0.0)),
                   MakeTimeAccessor (&PointToPointLaserNetDevice::SetInterframeGap,
                                     &PointToPointLaserNetDevice::GetInterframeGap),
                   MakeTimeChecker ())

    //
    // Traffic shaping: slots granted per period to each class.  The share of
    // the line rate available to a class is its share divided by the sum.
    //
    .AddAttribute ("TrafficShare",
                   "Number of slots per period granted to the normal queue "
                   "(0 switches the class off and gives the link to the other)",
                   UintegerValue (2),
                   MakeUintegerAccessor (&PointToPointLaserNetDevice::m_traffic_share),
                   MakeUintegerChecker<uint16_t> (0))
    .AddAttribute ("TrafficShareBroadcast",
                   "Number of slots per period granted to the broadcast queue "
                   "(0 switches the class off and gives the link to the other)",
                   UintegerValue (1),
                   MakeUintegerAccessor (&PointToPointLaserNetDevice::m_traffic_share_broadcast),
                   MakeUintegerChecker<uint16_t> (0))

    //
    // Trace sources at the "top" of the net device, where packets transition
    // to/from higher layers.
    //
    .AddTraceSource ("MacTx", 
                     "Trace source indicating a packet has arrived "
                     "for transmission by this device",
                     MakeTraceSourceAccessor (&PointToPointLaserNetDevice::m_macTxTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("MacTxDrop", 
                     "Trace source indicating a packet has been dropped "
                     "by the device before transmission",
                     MakeTraceSourceAccessor (&PointToPointLaserNetDevice::m_macTxDropTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("MacPromiscRx", 
                     "A packet has been received by this device, "
                     "has been passed up from the physical layer "
                     "and is being forwarded up the local protocol stack.  "
                     "This is a promiscuous trace,",
                     MakeTraceSourceAccessor (&PointToPointLaserNetDevice::m_macPromiscRxTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("MacRx", 
                     "A packet has been received by this device, "
                     "has been passed up from the physical layer "
                     "and is being forwarded up the local protocol stack.  "
                     "This is a non-promiscuous trace,",
                     MakeTraceSourceAccessor (&PointToPointLaserNetDevice::m_macRxTrace),
                     "ns3::Packet::TracedCallback")
#if 0
    // Not currently implemented for this device
    .AddTraceSource ("MacRxDrop", 
                     "Trace source indicating a packet was dropped "
                     "before being forwarded up the stack",
                     MakeTraceSourceAccessor (&PointToPointLaserNetDevice::m_macRxDropTrace),
                     "ns3::Packet::TracedCallback")
#endif
    //
    // Trace sources at the "bottom" of the net device, where packets transition
    // to/from the channel.
    //
    .AddTraceSource ("PhyTxBegin", 
                     "Trace source indicating a packet has begun "
                     "transmitting over the channel",
                     MakeTraceSourceAccessor (&PointToPointLaserNetDevice::m_phyTxBeginTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("PhyTxEnd", 
                     "Trace source indicating a packet has been "
                     "completely transmitted over the channel",
                     MakeTraceSourceAccessor (&PointToPointLaserNetDevice::m_phyTxEndTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("PhyTxDrop", 
                     "Trace source indicating a packet has been "
                     "dropped by the device during transmission",
                     MakeTraceSourceAccessor (&PointToPointLaserNetDevice::m_phyTxDropTrace),
                     "ns3::Packet::TracedCallback")
#if 0
    // Not currently implemented for this device
    .AddTraceSource ("PhyRxBegin", 
                     "Trace source indicating a packet has begun "
                     "being received by the device",
                     MakeTraceSourceAccessor (&PointToPointLaserNetDevice::m_phyRxBeginTrace),
                     "ns3::Packet::TracedCallback")
#endif
    .AddTraceSource ("PhyRxEnd", 
                     "Trace source indicating a packet has been "
                     "completely received by the device",
                     MakeTraceSourceAccessor (&PointToPointLaserNetDevice::m_phyRxEndTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("PhyRxDrop", 
                     "Trace source indicating a packet has been "
                     "dropped by the device during reception",
                     MakeTraceSourceAccessor (&PointToPointLaserNetDevice::m_phyRxDropTrace),
                     "ns3::Packet::TracedCallback")

    //
    // Trace sources designed to simulate a packet sniffer facility (tcpdump).
    // Note that there is really no difference between promiscuous and 
    // non-promiscuous traces in a point-to-point link.
    //
    .AddTraceSource ("Sniffer", 
                    "Trace source simulating a non-promiscuous packet sniffer "
                     "attached to the device",
                     MakeTraceSourceAccessor (&PointToPointLaserNetDevice::m_snifferTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("PromiscSniffer", 
                     "Trace source simulating a promiscuous packet sniffer "
                     "attached to the device",
                     MakeTraceSourceAccessor (&PointToPointLaserNetDevice::m_promiscSnifferTrace),
                     "ns3::Packet::TracedCallback")
  ;
  return tid;
}


PointToPointLaserNetDevice::PointToPointLaserNetDevice () 
  :
    m_txMachineState (READY),
    m_channel (0),
    m_linkUp (false),
    m_mtu (DEFAULT_MTU),
    m_currentPkt (0),
    m_slotTime (Seconds (0)),
    m_nextSlot (0),
    m_armedSlot (0)
{
  NS_LOG_FUNCTION (this);
}

PointToPointLaserNetDevice::~PointToPointLaserNetDevice ()
{
  NS_LOG_FUNCTION (this);
}

void
PointToPointLaserNetDevice::AddHeader (Ptr<Packet> p, uint16_t protocolNumber)
{
  NS_LOG_FUNCTION (this << p << protocolNumber);
  PppHeader ppp;
  ppp.SetProtocol (EtherToPpp (protocolNumber));
  p->AddHeader (ppp);
}

bool
PointToPointLaserNetDevice::ProcessHeader (Ptr<Packet> p, uint16_t& param)
{
  NS_LOG_FUNCTION (this << p << param);
  PppHeader ppp;
  p->RemoveHeader (ppp);
  param = PppToEther (ppp.GetProtocol ());
  return true;
}

void
PointToPointLaserNetDevice::DoDispose ()
{
  NS_LOG_FUNCTION (this);
  m_slotEvent.Cancel ();
  m_node = 0;
  m_channel = 0;
  m_receiveErrorModel = 0;
  m_currentPkt = 0;
  m_queue = 0;
  m_queue_broadcast = 0;
  NetDevice::DoDispose ();
}

void
PointToPointLaserNetDevice::SetDataRate (DataRate bps)
{
  NS_LOG_FUNCTION (this);
  m_bps = bps;
  UpdateSlotTime ();
}

DataRate
PointToPointLaserNetDevice::GetDataRate (void) const
{
  NS_LOG_FUNCTION (this);
  return m_bps;
}

void
PointToPointLaserNetDevice::SetInterframeGap (Time t)
{
  NS_LOG_FUNCTION (this << t.GetSeconds ());
  m_tInterframeGap = t;
  UpdateSlotTime ();
}

Time
PointToPointLaserNetDevice::GetInterframeGap (void) const
{
  NS_LOG_FUNCTION (this);
  return m_tInterframeGap;
}

void
PointToPointLaserNetDevice::UpdateSlotTime (void)
{
  NS_LOG_FUNCTION (this);

  if (m_bps.GetBitRate () == 0 || m_mtu == 0)
    {
      // Not all attributes have been applied yet; we will be called again.
      return;
    }

  //
  // One frame per slot, and the slot is sized for the largest legal frame.
  // A class owning k of every P slots can therefore never exceed k/P of the
  // line rate over any window, whatever the actual packet sizes are.
  //
   //
  // Computed in integer Time ticks rather than via CalculateBytesTxTime():
  // that goes through int64x64_t fixed-point and can land one tick low on
  // values that are not binary-representable, which would silently shift
  // every rate in the simulation.
  //
  int64_t frameBits      = (int64_t) (m_mtu + PPP_HEADER_BYTES) * 8;
  int64_t ticksPerSecond = Seconds (1).GetTimeStep ();
  int64_t num            = frameBits * ticksPerSecond;
  int64_t rate           = (int64_t) m_bps.GetBitRate ();

  //
  // The slot must be a whole number of Time ticks, or the grid drifts and
  // SlotTick()'s alignment check fires mid-run.
  //
  NS_ABORT_MSG_IF (num % rate != 0,
                   "PointToPointLaserNetDevice: slot time is not exactly "
                   "representable at the current Time resolution. Frame = "
                   << (m_mtu + PPP_HEADER_BYTES) << " B, rate = " << rate
                   << " bps. Adjust Mtu so that (Mtu + 2) * 8 * ticksPerSecond "
                      "is divisible by the bit rate.");

  m_slotTime = TimeStep (num / rate) + m_tInterframeGap;

  NS_LOG_LOGIC ("Slot time is now " << m_slotTime.As (Time::NS));
}

bool
PointToPointLaserNetDevice::IsNormalSlot (uint64_t n) const
{
  return (n % (uint64_t) (m_traffic_share + m_traffic_share_broadcast)) < m_traffic_share;
}

uint64_t
PointToPointLaserNetDevice::NextSlotForClass (uint64_t from, bool normalClass) const
{
  uint16_t share = normalClass ? m_traffic_share : m_traffic_share_broadcast;

  //
  // A share of zero means the class is switched off: it owns no slot at any
  // point on the grid, so the whole link goes to the other class.
  //
  if (share == 0)
    {
      return NO_SLOT;
    }

  uint32_t period = (uint32_t) m_traffic_share + (uint32_t) m_traffic_share_broadcast;
  uint32_t phase = (uint32_t) (from % period);

  if (normalClass)
    {
      // Normal slots are [0, m_traffic_share) within each period.
      return (phase < m_traffic_share) ? from : from + (period - phase);
    }

  // Broadcast slots are [m_traffic_share, period) within each period.
  return (phase >= m_traffic_share) ? from : from + (m_traffic_share - phase);
}

void
PointToPointLaserNetDevice::ArmShaper (void)
{
  NS_LOG_FUNCTION (this);

  //
  // A slot is in flight.  TransmitComplete() fires exactly on the following
  // slot boundary and will re-arm from there, so there is nothing to do.
  //
  if (m_txMachineState != READY)
    {
      return;
    }

  NS_ABORT_MSG_IF (m_slotTime.IsZero (),
                   "PointToPointLaserNetDevice: slot time is zero "
                   "(DataRate or Mtu not configured?)");

  //
  // Exactly one share may be zero: that switches its class off and gives the
  // whole link to the other one.  Both zero leaves no period at all and would
  // divide by zero in the slot arithmetic.
  //
  NS_ABORT_MSG_IF (m_traffic_share == 0 && m_traffic_share_broadcast == 0,
                   "PointToPointLaserNetDevice: TrafficShare and "
                   "TrafficShareBroadcast cannot both be zero");

  bool haveA = (m_queue != nullptr && m_queue->GetNPackets () != 0);
  bool haveB = (m_queue_broadcast != nullptr && m_queue_broadcast->GetNPackets () != 0);

  if (!haveA && !haveB)
    {
      // Idle.  Cost nothing until the next Send().
      m_slotEvent.Cancel ();
      return;
    }

  //
  // Never reuse a slot that has already started: the frame would run past the
  // boundary and steal capacity from the next class.  All grid arithmetic is
  // done on raw time steps so that it is exact and free of any rounding.
  //
  int64_t nowTicks = Simulator::Now ().GetTimeStep ();
  int64_t slotTicks = m_slotTime.GetTimeStep ();

  uint64_t nowSlot = (uint64_t) (nowTicks / slotTicks);
  if (slotTicks * (int64_t) nowSlot < nowTicks)
    {
      nowSlot++;   // we are inside a slot, not on its boundary
    }
  if (nowSlot > m_nextSlot)
    {
      m_nextSlot = nowSlot;
    }

  //
  // Earliest usable slot over the backlogged classes.  This skips a whole run
  // of unusable slots in one step, so an idle class costs a single event and
  // not one event per slot.
  //
  uint64_t cand = NO_SLOT;
  if (haveA)
    {
      cand = std::min (cand, NextSlotForClass (m_nextSlot, true));
    }
  if (haveB)
    {
      cand = std::min (cand, NextSlotForClass (m_nextSlot, false));
    }

  //
  // The only backlogged class has a share of zero, so it is switched off and
  // owns no slot.  Its packets stay queued (and are dropped on overflow),
  // which is the honest consequence of allocating it no bandwidth.
  //
  if (cand == NO_SLOT)
    {
      m_slotEvent.Cancel ();
      return;
    }

  int64_t whenTicks = slotTicks * (int64_t) cand;

  if (whenTicks == nowTicks)
    {
      // We are standing on the boundary: no event needed at all.
      m_slotEvent.Cancel ();
      m_nextSlot = cand;
      StartSlot ();
      return;
    }

  //
  // A pending wake-up may have become stale: the other class can have just
  // become backlogged and own an earlier slot.  But in the common case the
  // arriving packet joins a class that is already scheduled, and re-arming
  // would only litter the event queue with cancelled events.
  //
  // NOTE: on ns-3.36 and later, EventId::IsRunning() is spelled IsPending().
  //
  if (m_slotEvent.IsPending() && m_armedSlot == cand)
    {
      return;
    }

  m_slotEvent.Cancel ();
  m_armedSlot = cand;
  m_slotEvent = Simulator::Schedule (TimeStep (whenTicks - nowTicks),
                                     &PointToPointLaserNetDevice::SlotTick, this);
}

void
PointToPointLaserNetDevice::SlotTick (void)
{
  NS_LOG_FUNCTION (this);

  NS_ABORT_MSG_IF (Simulator::Now ().GetTimeStep ()
                   != m_slotTime.GetTimeStep () * (int64_t) m_armedSlot,
                 "Slot grid drift: check that Time::SetResolution (Time::PS) was called");

 // NS_ASSERT_MSG (m_txMachineState == READY, "Slot boundary reached while transmitting");
if (m_txMachineState != READY)
  {
    std::cerr << "skipped slot at t=" << Simulator::Now ().GetSeconds ()
              << "  now=" << Simulator::Now ().GetTimeStep ()
              << "  slot=" << m_slotTime.GetTimeStep ()
              << "  armed=" << m_armedSlot
              << "  next=" << m_nextSlot
              << "  txStart=" << m_txStartTicks.GetTimeStep ()
              << "  mpi_if=" << MpiInterface::GetSystemId()
              << std::endl;
    return;
  }

  m_nextSlot = m_armedSlot;
  StartSlot ();
}

void
PointToPointLaserNetDevice::StartSlot (void)
{
  NS_LOG_FUNCTION (this);

  Ptr<Queue<Packet> > q = IsNormalSlot (m_nextSlot) ? m_queue : m_queue_broadcast;
  Ptr<Packet> p = q->Dequeue ();

  //
  // NS_ABORT rather than NS_ASSERT: asserts are compiled out in optimized
  // builds, and reaching here with an empty queue means the slot arithmetic
  // and the backlog test have disagreed.  Continuing would transmit a null
  // packet.
  //
  NS_ABORT_MSG_IF (p == nullptr,
                   "PointToPointLaserNetDevice: slot " << m_nextSlot
                   << " served for an empty queue (TrafficShare="
                   << m_traffic_share << ", TrafficShareBroadcast="
                   << m_traffic_share_broadcast << ")");

  m_nextSlot++;   // consumed
  
  // CHANGE
  TransmitStart (p);
}

bool
PointToPointLaserNetDevice::TransmitStart (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << p);
  NS_LOG_LOGIC ("UID is " << p->GetUid () << ")");

  //
  // This function is called to start the process of transmitting a packet.
  // We need to tell the channel that we've started wiggling the wire and
  // schedule an event that will be executed when the transmission is complete.
  //
  NS_ASSERT_MSG (m_txMachineState == READY, "Must be READY to transmit");
  m_txMachineState = BUSY;

  m_slotEvent.Cancel (); // CHANGE

  m_currentPkt = p;

  m_snifferTrace (p); // CHANGE
  m_promiscSnifferTrace (p); // CHANGE

  m_phyTxBeginTrace (m_currentPkt);
  TrackUtilization(true);

  Time txTime = m_bps.CalculateBytesTxTime (p->GetSize ());

  //
  // The transmitter is committed for the whole slot, not merely for this
  // frame's bits: that is what keeps the per-class rate bound independent of
  // the packet size.  The channel still sees the real on-air time, so
  // reception timing is unaffected.
  //
  NS_LOG_LOGIC ("Schedule TransmitCompleteEvent in " << m_slotTime.GetSeconds () << "sec");
  Simulator::Schedule (m_slotTime, &PointToPointLaserNetDevice::TransmitComplete, this);

  // CHANGE:
  m_txStartTicks = Simulator::Now();

  bool result = m_channel->TransmitStart (p, this, m_destination_node, txTime);
  if (result == false)
    {
      m_phyTxDropTrace (p);
    }
  return result;
}

void
PointToPointLaserNetDevice::TransmitComplete (void)
{
  NS_LOG_FUNCTION (this);

  //
  // This function is called when the slot that carried a packet has ended.
  // Simulator::Now() is therefore exactly on a slot boundary, and we hand
  // control straight back to the shaper.
  //
  NS_ASSERT_MSG (m_txMachineState == BUSY, "Must be BUSY if transmitting");
  m_txMachineState = READY;

  NS_ASSERT_MSG (m_currentPkt != nullptr, "PointToPointLaserNetDevice::TransmitComplete(): m_currentPkt zero");

  m_phyTxEndTrace (m_currentPkt);
  TrackUtilization(false);
  m_currentPkt = 0;

  //
  // If the next slot belongs to a backlogged class, ArmShaper() starts it
  // immediately without creating an event.  A saturated link therefore costs
  // exactly one event per packet, the same as the stock device.
  //
  ArmShaper ();
}

bool
PointToPointLaserNetDevice::Attach (Ptr<PointToPointLaserChannel> ch)
{
  NS_LOG_FUNCTION (this << &ch);

  m_channel = ch;

  m_channel->Attach (this);

  //
  // This device is up whenever it is attached to a channel.  A better plan
  // would be to have the link come up when both devices are attached, but this
  // is not done for now.
  //
  NotifyLinkUp ();
  return true;
}

void
PointToPointLaserNetDevice::SetQueue (Ptr<Queue<Packet> > q)
{
  NS_LOG_FUNCTION (this << q);
  m_queue = q;
}

Ptr<Queue<Packet> >
PointToPointLaserNetDevice::GetQueue (void) const
{ 
  NS_LOG_FUNCTION (this);
  return m_queue;
}

void
PointToPointLaserNetDevice::SetBroadcastQueue (Ptr<Queue<Packet> > q)
{
  NS_LOG_FUNCTION (this << q);
  m_queue_broadcast = q;
}

Ptr<Queue<Packet> >
PointToPointLaserNetDevice::GetBroadcastQueue (void) const
{
  NS_LOG_FUNCTION (this);
  return m_queue_broadcast;
}

void
PointToPointLaserNetDevice::SetReceiveErrorModel (Ptr<ErrorModel> em)
{
  NS_LOG_FUNCTION (this << em);
  m_receiveErrorModel = em;
}

void
PointToPointLaserNetDevice::Receive (Ptr<Packet> packet)
{
  NS_LOG_FUNCTION (this << packet);
  uint16_t protocol = 0;

  if (m_receiveErrorModel && m_receiveErrorModel->IsCorrupt (packet) ) 
    {
      // 
      // If we have an error model and it indicates that it is time to lose a
      // corrupted packet, don't forward this packet up, let it go.
      //
      NS_LOG_LOGIC ("Dropping corrupted packet");
      m_phyRxDropTrace (packet);
    }
  else 
    {
      // 
      // Hit the trace hooks.  All of these hooks are in the same place in this 
      // device because it is so simple, but this is not usually the case in
      // more complicated devices.
      //
      m_snifferTrace (packet);
      m_promiscSnifferTrace (packet);
      m_phyRxEndTrace (packet);

      //
      // Trace sinks will expect complete packets, not packets without some of the
      // headers.
      //
      Ptr<Packet> originalPacket = packet->Copy ();

      //
      // Strip off the point-to-point protocol header and forward this packet
      // up the protocol stack.  Since this is a simple point-to-point link,
      // there is no difference in what the promisc callback sees and what the
      // normal receive callback sees.
      //
      ProcessHeader (packet, protocol);

      if (!m_promiscCallback.IsNull ())
        {
          m_macPromiscRxTrace (originalPacket);
          m_promiscCallback (this, packet, protocol, GetRemote (), GetAddress (), NetDevice::PACKET_HOST);
        }

      m_macRxTrace (originalPacket);
      m_rxCallback (this, packet, protocol, GetRemote ());
    }
}

void
PointToPointLaserNetDevice::NotifyLinkUp (void)
{
  NS_LOG_FUNCTION (this);
  m_linkUp = true;
  m_linkChangeCallbacks ();
}

void
PointToPointLaserNetDevice::SetIfIndex (const uint32_t index)
{
  NS_LOG_FUNCTION (this);
  m_ifIndex = index;
}

uint32_t
PointToPointLaserNetDevice::GetIfIndex (void) const
{
  return m_ifIndex;
}

Ptr<Channel>
PointToPointLaserNetDevice::GetChannel (void) const
{
  return m_channel;
}

//
// This is a point-to-point device, so we really don't need any kind of address
// information.  However, the base class NetDevice wants us to define the
// methods to get and set the address.  Rather than be rude and assert, we let
// clients get and set the address, but simply ignore them.

void
PointToPointLaserNetDevice::SetAddress (Address address)
{
  NS_LOG_FUNCTION (this << address);
  m_address = Mac48Address::ConvertFrom (address);
}

Address
PointToPointLaserNetDevice::GetAddress (void) const
{
  return m_address;
}

void
PointToPointLaserNetDevice::SetDestinationNode (Ptr<Node> node)
{
  NS_LOG_FUNCTION (this << node);
  m_destination_node = node;
}

Ptr<Node>
PointToPointLaserNetDevice::GetDestinationNode (void) const
{
  return m_destination_node;
}

bool
PointToPointLaserNetDevice::IsLinkUp (void) const
{
  NS_LOG_FUNCTION (this);
  return m_linkUp;
}

void
PointToPointLaserNetDevice::AddLinkChangeCallback (Callback<void> callback)
{
  NS_LOG_FUNCTION (this);
  m_linkChangeCallbacks.ConnectWithoutContext (callback);
}

//
// This is a point-to-point device, so every transmission is a broadcast to
// all of the devices on the network.
//
bool
PointToPointLaserNetDevice::IsBroadcast (void) const
{
  NS_LOG_FUNCTION (this);
  return true;
}

//
// We don't really need any addressing information since this is a 
// point-to-point device.  The base class NetDevice wants us to return a
// broadcast address, so we make up something reasonable.
//
Address
PointToPointLaserNetDevice::GetBroadcast (void) const
{
  NS_LOG_FUNCTION (this);
  return Mac48Address ("ff:ff:ff:ff:ff:ff");
}

bool
PointToPointLaserNetDevice::IsMulticast (void) const
{
  NS_LOG_FUNCTION (this);
  return true;
}

Address
PointToPointLaserNetDevice::GetMulticast (Ipv4Address multicastGroup) const
{
  NS_LOG_FUNCTION (this);
  return Mac48Address ("01:00:5e:00:00:00");
}

Address
PointToPointLaserNetDevice::GetMulticast (Ipv6Address addr) const
{
  NS_LOG_FUNCTION (this << addr);
  return Mac48Address ("33:33:00:00:00:00");
}

bool
PointToPointLaserNetDevice::IsPointToPoint (void) const
{
  NS_LOG_FUNCTION (this);
  return true;
}

bool
PointToPointLaserNetDevice::IsBridge (void) const
{
  NS_LOG_FUNCTION (this);
  return false;
}

bool
PointToPointLaserNetDevice::Send (
  Ptr<Packet> packet, 
  const Address &dest, 
  uint16_t protocolNumber)
{
  NS_LOG_FUNCTION (this << packet << dest << protocolNumber);
  NS_LOG_LOGIC ("p=" << packet << ", dest=" << &dest);
  NS_LOG_LOGIC ("UID is " << packet->GetUid ());

  //
  // If IsLinkUp() is false it means there is no channel to send any packet 
  // over so we just hit the drop trace on the packet and return an error.
  //
  if (IsLinkUp () == false)
    {
      m_macTxDropTrace (packet);
      return false;
    }

  //
  // Stick a point to point protocol header on the packet in preparation for
  // shoving it out the door.
  //
  AddHeader (packet, protocolNumber);

  m_macTxTrace (packet);

  //
  // A frame longer than one slot would overrun into the next class's slot and
  // break the rate bound, so this is a hard error rather than a silent drop.
  //
  uint32_t maxFrame = m_mtu + PPP_HEADER_BYTES;
  NS_ABORT_MSG_IF (packet->GetSize () > maxFrame,
                   "PointToPointLaserNetDevice: frame of " << packet->GetSize ()
                   << " bytes exceeds the slot capacity of " << maxFrame
                   << " bytes; the per-class rate bound would be violated");

  //
  // Select the transmit queue from the protocol number.  Anything unrecognised
  // goes to the normal queue (and would already have tripped EtherToPpp).
  //
  Ptr<Queue<Packet> > queue;
  switch (protocolNumber)
    {
    case PROT_TYPE_A:
      queue = m_queue;
      break;
    case PROT_TYPE_B:
      queue = m_queue_broadcast;
      break;
    default:
      queue = m_queue;
      break;
    }

  NS_ABORT_MSG_IF (queue == nullptr, "PointToPointLaserNetDevice: transmit queue not set");

  if (queue->Enqueue (packet) == false)
    {
      // Enqueue may fail (overflow)
      NS_LOG_LOGIC ("Dropping packet: transmit queue overflow");
      m_macTxDropTrace (packet);
      return false;
    }

  //
  // Never transmit from here: only the shaper may dequeue, and only on a slot
  // boundary.  ArmShaper() is a no-op if a transmission is already in flight
  // or if the correct wake-up is already scheduled.
  //
  ArmShaper ();
  return true;
}

bool
PointToPointLaserNetDevice::SendFrom (Ptr<Packet> packet, 
                                 const Address &source, 
                                 const Address &dest, 
                                 uint16_t protocolNumber)
{
  NS_LOG_FUNCTION (this << packet << source << dest << protocolNumber);
  return false;
}

Ptr<Node>
PointToPointLaserNetDevice::GetNode (void) const
{
  return m_node;
}

void
PointToPointLaserNetDevice::SetNode (Ptr<Node> node)
{
  NS_LOG_FUNCTION (this);
  m_node = node;
}

bool
PointToPointLaserNetDevice::NeedsArp (void) const
{
  NS_LOG_FUNCTION (this);
  return false;
}

void
PointToPointLaserNetDevice::SetReceiveCallback (NetDevice::ReceiveCallback cb)
{
  m_rxCallback = cb;
}

void
PointToPointLaserNetDevice::SetPromiscReceiveCallback (NetDevice::PromiscReceiveCallback cb)
{
  m_promiscCallback = cb;
}

bool
PointToPointLaserNetDevice::SupportsSendFrom (void) const
{
  NS_LOG_FUNCTION (this);
  return false;
}

void
PointToPointLaserNetDevice::DoMpiReceive (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << p);
  Receive (p);
}

Address 
PointToPointLaserNetDevice::GetRemote (void) const
{
  NS_LOG_FUNCTION (this);
  NS_ASSERT (m_channel->GetNDevices () == 2);
  for (std::size_t i = 0; i < m_channel->GetNDevices (); ++i)
    {
      Ptr<NetDevice> tmp = m_channel->GetDevice (i);
      if (tmp != this)
        {
          return tmp->GetAddress ();
        }
    }
  NS_ASSERT (false);
  // quiet compiler.
  return Address ();
}

bool
PointToPointLaserNetDevice::SetMtu (uint16_t mtu)
{
  NS_LOG_FUNCTION (this << mtu);
  m_mtu = mtu;
  UpdateSlotTime ();
  return true;
}

uint16_t
PointToPointLaserNetDevice::GetMtu (void) const
{
  NS_LOG_FUNCTION (this);
  return m_mtu;
}

uint16_t
PointToPointLaserNetDevice::PppToEther (uint16_t proto)
{
  NS_LOG_FUNCTION_NOARGS();
  switch(proto)
    {
    case 0x0021: return PROT_TYPE_A;   // IPv4      -> normal queue
    case 0x0022: return PROT_TYPE_B;   // class B   -> broadcast queue
    case 0x0057: return 0x86DD;        // IPv6
    default: NS_ASSERT_MSG (false, "PPP Protocol number not defined!");
    }
  return 0;
}

uint16_t
PointToPointLaserNetDevice::EtherToPpp (uint16_t proto)
{
  NS_LOG_FUNCTION_NOARGS();
  switch(proto)
    {
    case PROT_TYPE_A: return 0x0021;   // IPv4      -> normal queue
    case PROT_TYPE_B: return 0x0022;   // class B   -> broadcast queue
    case 0x86DD:      return 0x0057;   // IPv6
    default: NS_ASSERT_MSG (false, "PPP Protocol number not defined!");
    }
  return 0;
}

void
PointToPointLaserNetDevice::EnableUtilizationTracking(int64_t interval_ns) {
    m_utilization_tracking_enabled = true;
    m_interval_ns = interval_ns;
    m_prev_time_ns = 0;
    m_current_interval_start = 0;
    m_current_interval_end = m_interval_ns;
    m_idle_time_counter_ns = 0;
    m_busy_time_counter_ns = 0;
    m_current_state_is_on = false;
}

void
PointToPointLaserNetDevice::TrackUtilization(bool next_state_is_on) {
    if (m_utilization_tracking_enabled) {

        // Current time in nanoseconds
        int64_t now_ns = Simulator::Now().GetNanoSeconds();
        while (now_ns >= m_current_interval_end) {

            // Add everything until the end of the interval
            if (next_state_is_on) {
                m_idle_time_counter_ns += m_current_interval_end - m_prev_time_ns;
            } else {
                m_busy_time_counter_ns += m_current_interval_end - m_prev_time_ns;
            }

            // Save into the utilization array
            m_utilization.push_back(((double) m_busy_time_counter_ns) / ((double) m_interval_ns));

            // This must match up
            NS_ABORT_MSG_IF(m_idle_time_counter_ns + m_busy_time_counter_ns != m_interval_ns, "Not all time is accounted for");

            // Move to next interval
            m_idle_time_counter_ns = 0;
            m_busy_time_counter_ns = 0;
            m_prev_time_ns = m_current_interval_end;
            m_current_interval_start += m_interval_ns;
            m_current_interval_end += m_interval_ns;
        }

        // If not at the end of a new interval, just keep track of it all
        if (next_state_is_on) {
            m_idle_time_counter_ns += now_ns - m_prev_time_ns;
        } else {
            m_busy_time_counter_ns += now_ns - m_prev_time_ns;
        }

        // This has become the previous call
        m_current_state_is_on = next_state_is_on;
        m_prev_time_ns = now_ns;

    }
}

const std::vector<double>&
PointToPointLaserNetDevice::FinalizeUtilization() {
    TrackUtilization(!m_current_state_is_on);
    return m_utilization;
}

} // namespace ns3