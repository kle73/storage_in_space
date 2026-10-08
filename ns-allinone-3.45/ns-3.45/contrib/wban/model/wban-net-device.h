/*
 * Copyright (c) 2011 The Boeing Company
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Drishti Oza
 */
#ifndef LR_WPAN_NET_DEVICE_H
#define LR_WPAN_NET_DEVICE_H

#include "ns3/net-device.h"
#include "ns3/traced-callback.h"

namespace ns3
{

class SpectrumChannel;
class Node;

namespace wban
{
class WbanPhy;

/**
 * @class WbanNetDevice
 * @brief A network device for WBAN (Wireless Body Area Networks) in NS-3.
 *
 * This class implements the NetDevice abstraction tailored for WBAN scenarios.
 * It interacts with the WBAN PHY layer.
 */
class WbanNetDevice : public NetDevice
{
  public:
    /**
     * Get the type ID.
     *
     * @return the object TypeId
     */

    static TypeId GetTypeId();

    WbanNetDevice();
    ~WbanNetDevice() override;

    /**
     * How the pseudo-MAC address is built from
     * the short address (XXXX) and the PanId (YYYY).
     */
    enum PseudoMacAddressMode_e
    {
        RFC4944, //!< YYYY:0000:XXXX (with U/L bit set to local)
        RFC6282  //!< 0200:0000:XXXX
    };

    /**
     * Set the PHY to be used by the MAC and this NetDevice.
     *
     * @param phy the PHY to be used
     */

    void SetPhy(Ptr<WbanPhy> phy);
    /**
     * Set the channel to which the NetDevice, and therefore the PHY, should be
     * attached to.
     *
     * @param channel the channel to be used
     */

    void SetChannel(Ptr<SpectrumChannel> channel);

    /**
     * Get the PHY used by this NetDevice.
     *
     * @return the PHY object
     */

    Ptr<WbanPhy> GetPhy() const;

    // From class NetDevice
    void SetIfIndex(const uint32_t index) override;
    uint32_t GetIfIndex() const override;
    Ptr<Channel> GetChannel() const override;
    void SetAddress(Address address) override;
    Address GetAddress() const override;
    bool SetMtu(const uint16_t mtu) override;
    uint16_t GetMtu() const override;
    bool IsLinkUp() const override;
    void AddLinkChangeCallback(Callback<void> callback) override;
    bool IsBroadcast() const override;
    Address GetBroadcast() const override;
    bool IsMulticast() const override;
    Address GetMulticast(Ipv4Address multicastGroup) const override;
    Address GetMulticast(Ipv6Address addr) const override;
    bool IsBridge() const override;
    bool IsPointToPoint() const override;
    bool Send(Ptr<Packet> packet, const Address& dest, uint16_t protocolNumber) override;
    bool SendFrom(Ptr<Packet> packet,
                  const Address& source,
                  const Address& dest,
                  uint16_t protocolNumber) override;
    Ptr<Node> GetNode() const override;
    void SetNode(Ptr<Node> node) override;
    bool NeedsArp() const override;

    void SetReceiveCallback(NetDevice::ReceiveCallback cb) override;
    void SetPromiscReceiveCallback(PromiscReceiveCallback cb) override;
    bool SupportsSendFrom() const override;
    /**
     * Assign a fixed random variable stream number to the random variables
     * used by this model.  Return the number of streams that have been assigned.
     *
     * @param stream first stream index to use
     * @return the number of stream indices assigned by this model
     */

    int64_t AssignStreams(int64_t stream);

  private:
    // Inherited from NetDevice/Object
    void DoDispose() override;
    void DoInitialize() override;

    /**
     * Mark NetDevice link as up.
     */

    void LinkUp();
    /**
     * Mark NetDevice link as down.
     */

    void LinkDown();

    /**
     * Attribute accessor method for the "Channel" attribute.
     *
     * @return the channel to which this NetDevice is attached
     */

    Ptr<SpectrumChannel> DoGetChannel() const;

    /**
     * Configure PHY.
     */

    void CompleteConfig();

    /**
     * The PHY for this NetDevice.
     */

    Ptr<WbanPhy> m_phy;
    /**
     * The node associated with this NetDevice.
     */

    Ptr<Node> m_node;
    /**
     * True if PHY was successfully configured and the
     * NetDevice is ready for being used.
     */

    bool m_configComplete;
    /**
     * Is the link/device currently up and running?
     */

    bool m_linkUp;

    /**
     * The interface index of this NetDevice.
     */

    uint32_t m_ifIndex;

    /**
     * Trace source for link up/down changes.
     */

    TracedCallback<> m_linkChanges;

    /**
     * Upper layer callback used for notification of new data packet arrivals.
     */

    ReceiveCallback m_receiveCallback;
};
} // namespace wban
} // namespace ns3

#endif /* LR_WPAN_NET_DEVICE_H */
